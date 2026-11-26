#include "wifi_telemetry.h"
#include "tailwind_controller.h"
#include "wifi_page.h"

#include <Preferences.h>
#include <WiFi.h>
#include <esp_http_server.h>
#include <esp_random.h>
#include <esp_wifi.h>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
constexpr char ap_name[] = "Tailwind";
constexpr char ap_host[] = "192.168.4.1";
constexpr char ap_host_with_port[] = "192.168.4.1:80";
constexpr char ap_origin[] = "http://192.168.4.1";
constexpr char ap_origin_with_port[] = "http://192.168.4.1:80";
constexpr char password_key[] = "ap-pass";
constexpr char hex_digits[] = "0123456789abcdef";
constexpr char form_content_type[] = "application/x-www-form-urlencoded";
constexpr size_t password_length = 24;
constexpr unsigned long snapshot_expiry_ms = 1000;

const IPAddress ap_ip(192, 168, 4, 1);

bool loadAccessPointPassword(char (&password)[password_length + 1])
{
  Preferences preferences;
  if (!preferences.begin("tailwind-wifi", false)) {
    return false;
  }
  if (preferences.isKey(password_key)) {
    return preferences.getString(password_key, password, sizeof(password)) == sizeof(password) &&
           std::strspn(password, hex_digits) == password_length;
  }

  uint8_t random_bytes[password_length / 2];
  esp_fill_random(random_bytes, sizeof(random_bytes));
  for (size_t i = 0; i < sizeof(random_bytes); ++i) {
    password[2 * i] = hex_digits[random_bytes[i] >> 4];
    password[2 * i + 1] = hex_digits[random_bytes[i] & 0x0F];
  }
  password[password_length] = '\0';
  return preferences.putString(password_key, password) == password_length;
}

bool startAccessPoint()
{
  WiFi.persistent(false);
  if (!WiFi.mode(WIFI_AP)) {
    Serial.println("WiFi: AP mode failed");
    return false;
  }

  // Generate a password with the Wi-Fi radio active so the RNG has entropy.
  char password[password_length + 1] = {};
  if (!loadAccessPointPassword(password)) {
    Serial.println("WiFi: Could not load or store access point credentials");
    return false;
  }
  // Keep the ESP32's default SoftAP address and DHCP configuration.
  if (!WiFi.softAP(ap_name, password, 6, false, 2) || WiFi.softAPIP() != ap_ip) {
    Serial.println("WiFi: Access point setup failed");
    return false;
  }
  const esp_err_t bandwidth_result = esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
  if (bandwidth_result != ESP_OK) {
    Serial.printf("WiFi: HT20 setup failed (%s)\n", esp_err_to_name(bandwidth_result));
    return false;
  }

  Serial.printf("WiFi: %s at http://%s\n", ap_name, ap_host);
  Serial.printf("WiFi: Tailwind passphrase: %s\n", password);
  return true;
}

void logHeap(const char *event)
{
  Serial.printf("WiFi: %s heap free=%u min=%u max_block=%u bytes\n", event,
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getMinFreeHeap()),
                static_cast<unsigned>(ESP.getMaxAllocHeap()));
}

esp_err_t sendJson(httpd_req_t *request, const char *status, const char *body)
{
  httpd_resp_set_status(request, status);
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  httpd_resp_set_type(request, "application/json");
  return httpd_resp_send(request, body, HTTPD_RESP_USE_STRLEN);
}

struct JsonNumber
{
  char text[24];
};

JsonNumber formatMetric(float value, bool valid)
{
  JsonNumber result{"null"};
  if (valid && std::isfinite(value)) {
    std::snprintf(result.text, sizeof(result.text), "%.9g", static_cast<double>(value));
  }
  return result;
}

esp_err_t handleStatus(httpd_req_t *request)
{
  if (xSemaphoreTake(dataUpdateSemaphore, pdMS_TO_TICKS(2)) != pdTRUE) {
    return sendJson(request, "503 Service Unavailable", R"({"error":"status_unavailable"})");
  }
  const SharedSensorData sensor = sharedSensorData;
  const SharedVescData vesc = sharedVescData;
  xSemaphoreGive(dataUpdateSemaphore);

  const unsigned long now = millis();
  const bool sensor_fresh = sensor.last_update != 0 && now - sensor.last_update < snapshot_expiry_ms;
  const bool vesc_fresh = vesc.telemetry_valid && vesc.last_update != 0 &&
                          now - vesc.last_update < snapshot_expiry_ms;
  const bool mode_valid = sensor_fresh && sensor.current_mode >= MODE_URBAN &&
                          sensor.current_mode <= MODE_OFF;
  const char *motor_enabled = mode_valid ? (sensor.motor_enabled ? "true" : "false") : "null";

  char response[640];
  const int length = std::snprintf(
      response, sizeof(response),
      "{\"uptime_ms\":%lu,\"vesc_valid\":%s,\"mode\":%s,\"motor_enabled\":%s,"
      "\"speed_kmh\":%s,\"cadence_rpm\":%s,\"torque_nm\":%s,\"rider_w\":%s,"
      "\"target_motor_a\":%s,\"motor_a\":%s,\"battery_a\":%s,\"battery_v\":%s,"
      "\"battery_percent\":%s,\"duty\":%s,\"watt_hours\":%s,"
      "\"watt_hours_charged\":%s,\"temp_fet_c\":%s}",
      now, vesc_fresh ? "true" : "false",
      formatMetric(static_cast<float>(sensor.current_mode), mode_valid).text, motor_enabled,
      formatMetric(vesc.speed_kmh, vesc_fresh && vesc.data_valid).text,
      formatMetric(sensor.cadence_rpm, sensor_fresh).text,
      formatMetric(sensor.filtered_torque, sensor_fresh).text,
      formatMetric(sensor.rider_power_w, sensor_fresh).text,
      formatMetric(sensor.target_current, sensor_fresh).text,
      formatMetric(vesc.actual_current, vesc_fresh).text,
      formatMetric(vesc.input_current, vesc_fresh).text,
      formatMetric(vesc.battery_voltage, vesc_fresh).text,
      formatMetric(vesc.battery_percentage, vesc_fresh).text,
      formatMetric(vesc.duty_cycle, vesc_fresh).text,
      formatMetric(vesc.watt_hours, vesc_fresh).text,
      formatMetric(vesc.watt_hours_charged, vesc_fresh).text,
      formatMetric(vesc.temp_mosfet, vesc_fresh).text);
  if (length < 0 || static_cast<size_t>(length) >= sizeof(response)) {
    return sendJson(request, "503 Service Unavailable", R"({"error":"status_unavailable"})");
  }
  const esp_err_t result = sendJson(request, "200 OK", response);
  static bool first_snapshot = true;
  if (first_snapshot && result == ESP_OK) {
    logHeap("first status response");
    first_snapshot = false;
  }
  return result;
}

esp_err_t handleMode(httpd_req_t *request)
{
  char host[sizeof(ap_host_with_port)];
  char marker[2];
  if (httpd_req_get_hdr_value_str(request, "Host", host, sizeof(host)) != ESP_OK ||
      (std::strcmp(host, ap_host) != 0 && std::strcmp(host, ap_host_with_port) != 0) ||
      httpd_req_get_hdr_value_str(request, "X-Tailwind-Request", marker, sizeof(marker)) != ESP_OK ||
      std::strcmp(marker, "1") != 0) {
    return sendJson(request, "403 Forbidden", R"({"error":"forbidden"})");
  }

  char origin[sizeof(ap_origin_with_port)];
  if (httpd_req_get_hdr_value_len(request, "Origin") > 0 &&
      (httpd_req_get_hdr_value_str(request, "Origin", origin, sizeof(origin)) != ESP_OK ||
       (std::strcmp(origin, ap_origin) != 0 && std::strcmp(origin, ap_origin_with_port) != 0))) {
    return sendJson(request, "403 Forbidden", R"({"error":"forbidden"})");
  }

  char content_type[64];
  constexpr size_t media_type_length = sizeof(form_content_type) - 1;
  if (httpd_req_get_hdr_value_str(request, "Content-Type", content_type, sizeof(content_type)) != ESP_OK ||
      std::strncmp(content_type, form_content_type, media_type_length) != 0 ||
      (content_type[media_type_length] != '\0' && content_type[media_type_length] != ';')) {
    return sendJson(request, "415 Unsupported Media Type", R"({"error":"expected_urlencoded_mode"})");
  }

  char body[6];
  if (request->content_len != sizeof(body)) {
    return sendJson(request, "400 Bad Request", R"({"error":"invalid_mode"})");
  }
  size_t received = 0;
  while (received < sizeof(body)) {
    const int count = httpd_req_recv(request, body + received, sizeof(body) - received);
    if (count <= 0) {
      return ESP_FAIL;
    }
    received += count;
  }
  if (std::memcmp(body, "mode=", 5) != 0 || body[5] < '0' + MODE_URBAN || body[5] > '0' + MODE_OFF) {
    return sendJson(request, "400 Bad Request", R"({"error":"invalid_mode"})");
  }

  changeAssistMode(body[5] - '0');
  return sendJson(request, "202 Accepted", R"({"queued":true})");
}

esp_err_t handlePage(httpd_req_t *request)
{
  httpd_resp_set_type(request, "text/html; charset=utf-8");
  return httpd_resp_send(request, webInterface, HTTPD_RESP_USE_STRLEN);
}

bool startHttpServer()
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.task_priority = 1;
  config.stack_size = 8192;
  config.core_id = 1;
  config.max_open_sockets = 4;
  config.lru_purge_enable = true;
  config.recv_wait_timeout = 2;
  config.send_wait_timeout = 2;
  httpd_handle_t server = nullptr;
  if (httpd_start(&server, &config) != ESP_OK) {
    Serial.println("WiFi: HTTP server setup failed");
    return false;
  }

  const httpd_uri_t routes[] = {
    {"/", HTTP_GET, handlePage, nullptr},
    {"/api/status", HTTP_GET, handleStatus, nullptr},
    {"/api/mode", HTTP_POST, handleMode, nullptr},
  };
  for (const httpd_uri_t &route : routes) {
    if (httpd_register_uri_handler(server, &route) != ESP_OK) {
      Serial.println("WiFi: HTTP route setup failed");
      httpd_stop(server);
      return false;
    }
  }
  return true;
}

void wifiTelemetryTask(void *)
{
  vTaskDelay(pdMS_TO_TICKS(2000));
  while (!startAccessPoint() || !startHttpServer()) {
    WiFi.mode(WIFI_OFF);
    vTaskDelay(pdMS_TO_TICKS(10000));
  }

  logHeap("AP ready");
  vTaskDelete(nullptr);
}
}

void addLogMessage(const String &) {}
void addLogMessage(const char *) {}

void setupWifiTelemetry()
{
  if (xTaskCreatePinnedToCore(wifiTelemetryTask, "TailwindWiFi", 4096, nullptr, 1, nullptr, 1) != pdPASS) {
    Serial.println("WiFi: Failed to start dashboard task");
  }
}
