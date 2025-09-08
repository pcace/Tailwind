/*
  Name:    main.cpp - Tailwind Controller for VESC with Torque+PAS+Speed (Multi-Core)
  Created: 2025
  Author:  Johannes Lee-Frölich

  Description:
  Tailwind controller that combines ALL sensors (PAS + Torque + Speed)
  like Bosch, Shimano etc. Calculates human power and provides
  proportional, speed-dependent motor assistance.

  MAIN FUNCTIONS:
  - True Torque+PAS combination: Human Power = Torque × Cadence × 2π
  - Speed-dependent assist curves (like Bosch: max at 0km/h, min at 25km/h)
  - Linear interpolation between speed support points
  - Motor current is calculated dynamically, not fixed
  - Safety timeouts and sensor monitoring
  - Different assist modes selectable by reverse pedaling
  - Light control
  - Multi-Core Architecture with FreeRTOS
    - Core 0: Sensor Processing (PAS, Torque, Calculations) - HIGH PRIORITY
    - Core 1: VESC Communication (UART) - LOWER PRIORITY
    - Semaphore-based data synchronization

  Hardware:
  - ESP32 DevKit v1 (3.3V Logic, Dual Core)
  - Logic Level Converter for 5V sensors (IIC I2C 4-Channel 5V-3.3V)
  - VESC Motor Controller (Flipsky FESC 6.7 pro mini)
  - Hall sensors (PAS) for pedal detection on pins (interrupt capable)
  - Analog torque sensor (needs level converter if 5V sensor)
  - LED/Light
  - VESC Hardware Serial on UART2 via level converter
  - 48V 13S2P Battery, Q100C CST 36V350W Motor
*/

#include <Arduino.h>
#include <VescUart.h>
#include "tailwind_controller.h"

// WiFi Web Interface Integration
#include "wifi_telemetry.h"

// BLE (Bluetooth Low Energy) Interface Integration
#include "ble_telemetry.h"

// VESC Bridge Mode Integration
#include "vesc_bridge.h"

// ESP32 FreeRTOS Includes (native ESP32 Framework)
#ifdef ESP32
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#endif

// =============================================================================
// FREERTOS MULTI-CORE SETUP
// =============================================================================

// Task handles
TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t vescTaskHandle = NULL;

// Semaphores for thread-safe data sharing
SemaphoreHandle_t dataUpdateSemaphore = NULL;
SemaphoreHandle_t motorCommandSemaphore = NULL;

// Shared data structure instances (defined in header, instantiated here)
SharedSensorData sharedSensorData;
SharedVescData sharedVescData;
SharedMotorCommand sharedMotorCommand;

// =============================================================================
// VESC UART COMMUNICATION (Hardware Serial for ESP32)
// =============================================================================

/** VESC UART communication object */
VescUart vescUart;

// =============================================================================
// FREERTOS TASK FUNCTIONS
// =============================================================================

// CORE 0: Sensor Processing Task (HIGH PRIORITY)
void sensorTask(void *pvParameters)
{
  // Delay to ensure Serial is ready
  vTaskDelay(pdMS_TO_TICKS(100));

  Serial.println("=== SENSOR TASK STARTING ===");
  Serial.printf("Sensor Task running on Core: %d\n", xPortGetCoreID());

  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(10); // 100Hz update rate

  Serial.println("Sensor Task started on Core 0");

  // Add periodic status output for debugging
  unsigned long last_status = 0;

  for (;;)
  {
    // -------------------------------------------------------------------------
    // HIGH-FREQUENCY SENSOR PROCESSING (Core 0)
    // -------------------------------------------------------------------------

    // 0. Update debug simulation if enabled (must be called first)
    if (debug_mode)
    {
      update_debug_simulation();
    }

    // Debug output every 2 seconds
    if (millis() - last_status > 2000)
    {
      Serial.printf("[SENSOR] Task alive - Cadence: %.1f RPM, Torque: %.1f Nm (Raw: %d), Mode: %d, Motor: %s\n",
                    current_cadence_rpm, filtered_torque, raw_torque_value, current_mode, motor_enabled ? "ON" : "OFF");
      last_status = millis();
    }

    // 1. Read PAS sensors (interrupt-based, very fast)
    read_pas_sensors();

    // Special PAS debug output when pedaling is detected
    static float last_cadence = 0.0;
    static unsigned long last_pas_debug = 0;

    // Output PAS info when cadence changes significantly or periodically
    if (abs(current_cadence_rpm - last_cadence) > 2.0 ||
        (current_cadence_rpm > 0 && millis() - last_pas_debug > 1000))
    {
      if (current_cadence_rpm > 0)
      {
        // Serial.printf("[PAS] Pedaling detected! Cadence: %.1f RPM, Direction: %s, Position: %d\n",
        //              current_cadence_rpm,
        //              pedal_direction == 1 ? "FORWARD" : (pedal_direction == -1 ? "REVERSE" : "STOPPED"),
        //              pos);
        last_pas_debug = millis();
        last_cadence = current_cadence_rpm;
      }
    }
    else if (current_cadence_rpm == 0 && last_cadence > 0)
    {
      // Serial.println("[PAS] Pedaling stopped");
      last_cadence = 0.0;
    }

    // 2. Update cadence calculation
    update_cadence();

    // 3. Read and filter torque sensor
    update_torque();

    // 4. Mode management (reverse pedaling detection)
    update_mode_selection();

    // 5. Get current speed from VESC data (thread-safe)
    float current_speed = 0.0;
    bool speed_valid = false;
    if (xSemaphoreTake(dataUpdateSemaphore, pdMS_TO_TICKS(5)) == pdTRUE)
    {
      current_speed = sharedVescData.speed_kmh;
      speed_valid = sharedVescData.data_valid;
      xSemaphoreGive(dataUpdateSemaphore);
    }

    // 6. Calculate assist power with current speed
    current_speed_kmh = current_speed;
    vesc_data_valid = speed_valid;
    calculate_assist_power();

    // 7. Motor status and safety checks
    update_motor_status();

    // 8. Update shared sensor data (thread-safe)
    if (xSemaphoreTake(dataUpdateSemaphore, pdMS_TO_TICKS(10)) == pdTRUE)
    {
      sharedSensorData.cadence_rpm = current_cadence_rpm;
      sharedSensorData.cadence_rps = current_cadence_rps;
      sharedSensorData.torque_nm = crank_torque_nm;
      sharedSensorData.filtered_torque = filtered_torque;
      sharedSensorData.current_mode = current_mode;
      sharedSensorData.motor_enabled = motor_enabled;
      sharedSensorData.last_update = millis();
      xSemaphoreGive(dataUpdateSemaphore);
    }

    // 10. Send motor command (thread-safe)
    if (xSemaphoreTake(motorCommandSemaphore, pdMS_TO_TICKS(5)) == pdTRUE)
    {
      sharedMotorCommand.target_current = target_current_amps;
      sharedMotorCommand.command_ready = true;
      sharedMotorCommand.timestamp = millis();
      xSemaphoreGive(motorCommandSemaphore);
    }

    // Precise timing (100Hz)
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// CORE 1: VESC Communication Task (LOWER PRIORITY)
void vescTask(void *pvParameters)
{
  // Delay to ensure Serial is ready
  vTaskDelay(pdMS_TO_TICKS(200));

  Serial.println("=== VESC TASK STARTING ===");
  Serial.printf("VESC Task running on Core: %d\n", xPortGetCoreID());

  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(50); // 20Hz update rate (less critical)

  Serial.println("VESC Task started on Core 1");

  // Add periodic status output for debugging
  unsigned long last_status = 0;

  for (;;)
  {
    // -------------------------------------------------------------------------
    // VESC COMMUNICATION (Core 1) - Can block without affecting sensors
    // -------------------------------------------------------------------------

    // Debug output every 3 seconds
    if (millis() - last_status > 3000)
    {
      Serial.printf("[VESC] Task alive - Speed: %.1f km/h, Data valid: %s, Loop count: %d, Battery: %.1fV (%.2f%%)\n",
                    current_speed_kmh, vesc_data_valid ? "YES" : "NO", loopCounter, battery_voltage, battery_percentage);
      last_status = millis();
    }

    // 1. Query VESC data (speed, telemetry) - BLOCKING operation
    update_vesc_data(); // This can take 50ms without affecting Core 0!

    // 2. Update shared VESC data (thread-safe)
    if (xSemaphoreTake(dataUpdateSemaphore, pdMS_TO_TICKS(20)) == pdTRUE)
    {
      sharedVescData.speed_kmh = current_speed_kmh;
      sharedVescData.data_valid = vesc_data_valid;
      sharedVescData.actual_current = actual_current_amps;
      sharedVescData.battery_voltage = battery_voltage;
      sharedVescData.battery_percentage = battery_percentage;
      sharedVescData.last_update = millis();
      xSemaphoreGive(dataUpdateSemaphore);
    }

    // 3. Send motor command if ready (thread-safe)
    if (xSemaphoreTake(motorCommandSemaphore, pdMS_TO_TICKS(10)) == pdTRUE)
    {
      if (sharedMotorCommand.command_ready)
      {
        float command_current = sharedMotorCommand.target_current;

        // Send command to VESC (BLOCKING operation)
        if (motor_enabled)
        {
          vescUart.setCurrent(command_current);
        }
        else
        {
          vescUart.setCurrent(0.0);
        }

        sharedMotorCommand.command_ready = false;
      }
      xSemaphoreGive(motorCommandSemaphore);
    }

    // 4. Debug output (low frequency to avoid spam)
    static unsigned long last_debug = 0;
    if (millis() - last_debug > 500)
    { // Every 500ms
      print_debug_info();
      last_debug = millis();
    }

    // Update loop counter for compatibility
    loopCounter++;

    // Lower frequency for VESC communication (20Hz)
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// =============================================================================
// SETUP FUNCTION
// =============================================================================

void setup()
{
  // Enable debug serial (USB Serial)
  Serial.begin(115200);
  Serial.println("Starting Multi-Core Tailwind Controller (ESP32 DevKit v1)...");

  // =============================================================================
  // VESC BRIDGE MODE CHECK (PRIORITY 1)
  // =============================================================================
  if (vesc_bridge_mode)
  {
    Serial.println("===========================================");
    Serial.println("    VESC BRIDGE MODE ACTIVATED!");
    Serial.println("===========================================");
    Serial.println("Normal tailwind controller is DISABLED.");
    Serial.println("ESP32 will act as transparent VESC bridge.");
    Serial.println();

    // Initialize bridge mode and enter main loop
    initVescBridge();

    // Bridge mode main loop (never returns)
    while (true)
    {
      runVescBridge();
    }

    // This point should never be reached
    Serial.println("ERROR: Bridge mode unexpectedly exited!");
    return;
  }

  // =============================================================================
  // NORMAL TAILWIND CONTROLLER SETUP
  // =============================================================================
  Serial.println("Architecture: FreeRTOS Dual-Core");
  Serial.println("  - Core 0: Sensor Processing (HIGH PRIORITY, 100Hz)");
  Serial.println("  - Core 1: VESC Communication (LOWER PRIORITY, 20Hz)");

  // Initialize VESC Hardware Serial connection on ESP32
  // ESP32 has multiple hardware UARTs - using UART2
  Serial2.begin(115200); // UART2 for VESC communication
  vescUart.setSerialPort(&Serial2);

  Serial.println("VESC Serial initialized on Hardware UART2");

  // Initialize Tailwind system
  Serial.println("Calling tailwind_setup()...");
  tailwind_setup();
  Serial.println("tailwind_setup() completed successfully");

  // Create FreeRTOS semaphores for thread-safe data sharing
  Serial.println("Creating semaphores...");
  dataUpdateSemaphore = xSemaphoreCreateMutex();
  motorCommandSemaphore = xSemaphoreCreateMutex();

  if (dataUpdateSemaphore == NULL || motorCommandSemaphore == NULL)
  {
    Serial.println("ERROR: Failed to create semaphores!");
    while (1)
    {
      delay(1000);
    }
  }

  // Initialize shared data
  memset(&sharedSensorData, 0, sizeof(sharedSensorData));
  memset(&sharedVescData, 0, sizeof(sharedVescData));
  memset(&sharedMotorCommand, 0, sizeof(sharedMotorCommand));

  Serial.println("Semaphores created successfully");

  // Create FreeRTOS tasks on specific cores
  Serial.println("Creating FreeRTOS tasks...");

  // CORE 0: Sensor processing (HIGH PRIORITY)
  BaseType_t sensorTaskResult = xTaskCreatePinnedToCore(
      sensorTask,        // Task function
      "SensorTask",      // Task name
      4096,              // Stack size
      NULL,              // Parameter
      2,                 // Priority (HIGH)
      &sensorTaskHandle, // Task handle
      0                  // Core 0
  );

  // CORE 1: VESC communication (LOWER PRIORITY)
  BaseType_t vescTaskResult = xTaskCreatePinnedToCore(
      vescTask,        // Task function
      "VescTask",      // Task name
      4096,            // Stack size
      NULL,            // Parameter
      1,               // Priority (LOWER)
      &vescTaskHandle, // Task handle
      1                // Core 1
  );

  Serial.printf("Task creation results: Sensor=%d, VESC=%d\n", sensorTaskResult, vescTaskResult);

  if (sensorTaskHandle == NULL || vescTaskHandle == NULL)
  {
    Serial.println("ERROR: Failed to create tasks!");
    Serial.printf("SensorTask handle: %p\n", sensorTaskHandle);
    Serial.printf("VescTask handle: %p\n", vescTaskHandle);
    while (1)
    {
      delay(1000);
    }
  }

  // *** WiFi Web Interface Integration ***
  if (enable_wifi_telemetry)
  {
    Serial.println("Setting up WiFi Web Interface...");
    setupWifiTelemetry();
    Serial.println("WiFi Web Interface will start after WiFi connection");
    addLogMessage("Tailwind Controller started - Version: " + String(__DATE__));
  }

  // *** BLE (Bluetooth Low Energy) Interface Integration ***
  if (enable_ble_telemetry)
  {
    Serial.println("Setting up BLE Interface...");
    setupBLETelemetry();
    Serial.println("BLE Interface will start advertising");
    if (enable_wifi_telemetry)
    {
      addLogMessage("BLE Interface enabled - Device: " + String(BLE_DEVICE_NAME));
    }
  }

  Serial.println("Multi-Core tasks created successfully!");
  Serial.println("Setup complete - FreeRTOS scheduler will start tasks");

  // Setup is complete, FreeRTOS scheduler takes over
}

// =============================================================================
// MAIN LOOP (FreeRTOS tasks handle everything)
// =============================================================================

void loop()
{
  // The main loop is empty because FreeRTOS tasks handle everything

  // Optional: Watchdog feed or system monitoring could go here
  vTaskDelay(pdMS_TO_TICKS(1000)); // Sleep for 1 second

  // Optional: Monitor system health
  // static unsigned long last_health_check = 0;
  // if (millis() - last_health_check > 5000) {  // Every 5 seconds
  //   Serial.println("System Health Check:");
  //   Serial.printf("  - Free heap: %d bytes\n", ESP.getFreeHeap());
  //   Serial.printf("  - Sensor task stack: %d words free\n",
  //                 uxTaskGetStackHighWaterMark(sensorTaskHandle));
  //   Serial.printf("  - VESC task stack: %d words free\n",
  //                 uxTaskGetStackHighWaterMark(vescTaskHandle));
  //   last_health_check = millis();
  // }
}