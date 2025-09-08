/*
  WiFi Web Interface für ESP32 E-Bike Controller
  
  Dieses Modul läuft als separater Task auf Core 1 zusammen mit dem VESC Task.
  Es stellt ein Web-Interface zur Verfügung mit:
  - Live E-Bike Telemetrie-Daten
  - Log-Nachrichten Anzeige
  - TCP basiert (kein UDP)
  
  WICHTIG: 
  - Läuft auf Core 1 mit NIEDRIGER Priorität
  - Verwendet thread-safe Zugriff auf shared data
  - Web Interface ist über Browser erreichbar
*/

#include "wifi_telemetry.h"
#include "ebike_controller.h"

// External variables (defined in config.cpp)
extern int current_mode;

// Task handle
TaskHandle_t wifiTaskHandle = NULL;

// Web Server
WebServer webServer(WEB_SERVER_PORT);
bool wifiConnected = false;

// Leere Log-Funktionen (Stubs) um Kompatibilität zu erhalten
void addLogMessage(const String& message) {
  // Keine Implementierung - alle Logs werden ignoriert
}

void addLogMessage(const char* message) {
  // Keine Implementierung - alle Logs werden ignoriert
}

// Extrem einfache HTML-Seite (minimal für wenig Memory)
const char webInterface[] PROGMEM = R"HTML(<!DOCTYPE html>
<html>
<head>
    <title>E-Bike</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body{font-family:Arial;margin:20px;background:#f0f0f0}
        .card{background:white;padding:15px;margin:10px 0;border-radius:5px}
        .value{font-size:2em;font-weight:bold;color:#2c3e50;text-align:center}
        .label{font-size:0.9em;color:#666;text-align:center;margin-bottom:5px}
        .grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
        .btn{background:#3498db;color:white;border:none;padding:10px 20px;border-radius:5px;cursor:pointer;width:100%;margin:5px 0}
    </style>
</head>
<body>
    <h1>E-Bike Controller</h1>
    
    <div class="grid">
        <div class="card">
            <div class="label">Speed</div>
            <div class="value" id="speed">--</div>
            <div class="label">km/h</div>
        </div>
        <div class="card">
            <div class="label">Battery</div>
            <div class="value" id="battery">--</div>
            <div class="label">%</div>
        </div>
    </div>
    
    <div class="card">
        <div class="label">Torque</div>
        <div class="value" id="torque">--</div>
        <div class="label">Nm</div>
    </div>
    
    <div class="card">
        <div class="label">Mode</div>
        <div class="value" id="mode">--</div>
    </div>
    
    <button class="btn" onclick="updateData()">Refresh</button>

    <script>
        function updateData() {
            fetch('/status')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('speed').textContent = data.speed.toFixed(1);
                    document.getElementById('battery').textContent = data.battery.toFixed(0);
                    document.getElementById('torque').textContent = data.torque.toFixed(1);
                    document.getElementById('mode').textContent = data.mode;
                })
                .catch(error => console.error('Error:', error));
        }
        
        setInterval(updateData, 5000);
        updateData();
    </script>
</body>
</html>)HTML";

// Einfacher Status-Endpunkt (nur wichtigste Daten)
void handleStatusAPI() {
  // Minimale JSON-Antwort ohne ArduinoJson
  String response = "{";
  response += "\"speed\":" + String(sharedVescData.speed_kmh, 1) + ",";
  response += "\"battery\":" + String(sharedVescData.battery_percentage, 0) + ",";
  response += "\"mode\":" + String(sharedSensorData.current_mode) + ",";
  response += "\"torque\":" + String(sharedSensorData.filtered_torque, 1);
  response += "}";
  
  webServer.send(200, "application/json", response);
}

void wifiTelemetryTask(void *pvParameters) {
  // Delay um sicherzustellen dass andere Tasks schon laufen
  vTaskDelay(pdMS_TO_TICKS(2000));
  
  Serial.println("=== WiFi Web Interface Task Starting ===");
  
  // WiFi Access Point erstellen
  Serial.println("Creating WiFi Access Point...");
  
  // WiFi komplett stoppen und zurücksetzen
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  vTaskDelay(pdMS_TO_TICKS(100));
  
  // WiFi Access Point Mode setzen
  WiFi.mode(WIFI_AP);
  vTaskDelay(pdMS_TO_TICKS(100));
  
  // IP-Konfiguration setzen
  WiFi.softAPConfig(WIFI_AP_IP, WIFI_AP_GATEWAY, WIFI_AP_SUBNET);
  
  // Access Point starten
  bool apStarted = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CONNECTIONS);
  
  // Kurz warten und Status prüfen
  vTaskDelay(pdMS_TO_TICKS(500));

  if (apStarted) {
    Serial.println("WiFi Access Point created successfully!");
    Serial.printf("AP IP address: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("Web interface: http://%s\n", WiFi.softAPIP().toString().c_str());
    wifiConnected = true;
  } else {
    Serial.println("CRITICAL ERROR: Failed to create WiFi Access Point!");
    wifiConnected = false;
  }
  
  // Web Server Setup (nur bei erfolgreicher WiFi-Erstellung)
  if (wifiConnected) {
    // Route für Hauptseite
    webServer.on("/", HTTP_GET, []() {
      webServer.send_P(200, "text/html", webInterface);
    });
    
    // API Routes - nur ein einfacher Status-Endpunkt
    webServer.on("/status", HTTP_GET, handleStatusAPI);
    
    // 404 Handler
    webServer.onNotFound([]() {
      webServer.send(404, "text/plain", "404: Not Found");
    });
    
    webServer.begin();
    Serial.println("Web server started");
  }
  
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(TELEMETRY_UPDATE_RATE_MS);  for (;;) {
    // Web Server verarbeiten (minimal)
    if (wifiConnected) {
      webServer.handleClient();
    } else {
      // Falls AP aus irgendeinem Grund stoppt, versuchen neu zu starten
      static unsigned long lastAPCheck = 0;
      if (millis() - lastAPCheck > 10000) { // Alle 10 Sekunden prüfen
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        vTaskDelay(pdMS_TO_TICKS(200));
        WiFi.mode(WIFI_AP);
        vTaskDelay(pdMS_TO_TICKS(200));
        WiFi.softAPConfig(WIFI_AP_IP, WIFI_AP_GATEWAY, WIFI_AP_SUBNET);
        
        if (WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CONNECTIONS)) {
          wifiConnected = true;
        }
        lastAPCheck = millis();
      }
    }
    
    // Warten bis nächster Zyklus
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void setupWifiTelemetry() {
  Serial.println("Creating WiFi Web Interface Task...");
  
  // WiFi Task auf Core 1 erstellen (minimaler Stack)
  BaseType_t wifiTaskResult = xTaskCreatePinnedToCore(
    wifiTelemetryTask,        // Task function
    "WiFiWebTask",            // Task name
    8192,                     // Stack size (reduziert für minimale Anwendung)
    NULL,                     // Parameter
    1,                        // Priority (NIEDRIGERE als VESC Task)
    &wifiTaskHandle,          // Task handle
    1                         // Core 1 (gleicher Core wie VESC)
  );
  
  if (wifiTaskResult == pdPASS && wifiTaskHandle != NULL) {
    Serial.println("WiFi Web Interface Task created successfully!");
  } else {
    Serial.println("ERROR: Failed to create WiFi Web Interface Task!");
  }
}
