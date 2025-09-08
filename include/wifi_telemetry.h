#ifndef WIFI_TELEMETRY_H
#define WIFI_TELEMETRY_H

#include <Arduino.h>

#ifdef ESP32
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#endif

// WiFi Configuration - Access Point Mode
#define WIFI_AP_SSID "Tailwind"
#define WIFI_AP_PASSWORD "tailwind123"
#define WIFI_AP_CHANNEL 6 // Ändere zu Kanal 6 (weniger überfüllt)
#define WIFI_AP_MAX_CONNECTIONS 4
#define WIFI_AP_IP IPAddress(192, 168, 4, 1)
#define WIFI_AP_GATEWAY IPAddress(192, 168, 4, 1)
#define WIFI_AP_SUBNET IPAddress(255, 255, 255, 0)
#define WEB_SERVER_PORT 80
#define TELEMETRY_UPDATE_RATE_MS 2000 // 2Hz für weniger Last

// WiFi/Web Server task function
void wifiTelemetryTask(void *pvParameters);

// Setup function to create WiFi task
void setupWifiTelemetry();

// Log message functions (empty stubs - no actual logging)
void addLogMessage(const String &message);
void addLogMessage(const char *message);

// Global declarations for external access
extern TaskHandle_t wifiTaskHandle;
extern WebServer webServer;

#endif // WIFI_TELEMETRY_H
