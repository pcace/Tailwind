#ifndef BLE_TELEMETRY_H
#define BLE_TELEMETRY_H

#include <Arduino.h>

#ifdef ESP32
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/semphr.h"
  #include <BLEDevice.h>
  #include <BLEServer.h>
  #include <BLEUtils.h>
  #include <BLE2902.h>
  #include <ArduinoJson.h>
#endif

// BLE Configuration
#define BLE_DEVICE_NAME "E-Bike-Controller"
#define BLE_MANUFACTURER "OpenSource E-Bike"
#define BLE_MODEL_NUMBER "ESP32-Controller-v1.0"
#define BLE_FIRMWARE_VERSION "1.0.0"

// BLE Service UUIDs (Custom UUIDs for E-Bike Controller)
#define BLE_SERVICE_UUID_TELEMETRY     "12345678-1234-1234-1234-123456789ABC"
#define BLE_SERVICE_UUID_EXTENDED      "12345678-1234-1234-1234-123456789E1D"  // Fixed: Match mobile app expectation
#define BLE_SERVICE_UUID_CONTROL       "12345678-1234-1234-1234-123456789DEF"
#define BLE_SERVICE_UUID_DEVICE_INFO   "180A"  // Standard Device Information Service

// Telemetry Characteristics UUIDs
#define BLE_CHAR_UUID_SPEED            "12345678-1234-1234-1234-12345678A001"
#define BLE_CHAR_UUID_CADENCE          "12345678-1234-1234-1234-12345678A002"
#define BLE_CHAR_UUID_TORQUE           "12345678-1234-1234-1234-12345678A003"
#define BLE_CHAR_UUID_BATTERY          "12345678-1234-1234-1234-12345678A004"
#define BLE_CHAR_UUID_CURRENT          "12345678-1234-1234-1234-12345678A005"
#define BLE_CHAR_UUID_VESC_DATA        "12345678-1234-1234-1234-12345678A006"
#define BLE_CHAR_UUID_SYSTEM_STATUS    "12345678-1234-1234-1234-12345678A007"
#define BLE_CHAR_UUID_POWER_DATA       "12345678-1234-1234-1234-12345678A008"
#define BLE_CHAR_UUID_TEMPERATURES     "12345678-1234-1234-1234-12345678A009"
#define BLE_CHAR_UUID_COMPLETE_TELEMETRY "12345678-1234-1234-1234-12345678A010"

// Control Characteristics UUIDs
#define BLE_CHAR_UUID_MODE_CONTROL     "12345678-1234-1234-1234-12345678B001"
#define BLE_CHAR_UUID_MODE_LIST        "12345678-1234-1234-1234-12345678B002"
#define BLE_CHAR_UUID_COMMAND          "12345678-1234-1234-1234-12345678B003"

// Device Information Characteristics UUIDs (Standard)
#define BLE_CHAR_UUID_MANUFACTURER     "2A29"
#define BLE_CHAR_UUID_MODEL_NUMBER     "2A24"
#define BLE_CHAR_UUID_FIRMWARE_REV     "2A26"

// BLE Task Configuration
#define BLE_UPDATE_RATE_MS 2000         // 0.5Hz für BLE Telemetrie (weniger frequent als WiFi)
#define BLE_TASK_STACK_SIZE 12288       // Increased from 8192 to 12288 to prevent stack overflow
#define BLE_TASK_PRIORITY 1             // Niedrige Priorität auf Core 1

// BLE Data Structures
struct BLETelemetryData {
    // Basic sensor data
    float speed;                    // current_speed_kmh
    float cadence;                  // current_cadence_rpm  
    float torque;                   // filtered_torque
    float raw_torque;               // raw_torque_value
    
    // Power and current
    float human_power;              // human_power_watts
    float assist_power;             // assist_power_watts
    float motor_current_target;     // target_current_amps
    float motor_current_actual;     // actual_current_amps
    float motor_rpm;                // current_motor_rpm
    
    // Battery data
    float battery_voltage;          // battery_voltage
    float battery_percentage;       // battery_percentage
    bool battery_low;               // battery_low
    bool battery_critical;          // battery_critical
    
    // System status
    uint8_t current_mode;           // current_mode
    bool motor_enabled;             // motor_enabled
    bool light_on;                  // lightOn
    bool vesc_data_valid;           // vesc_data_valid
    float dynamic_assist_factor;    // dynamic_assist_factor
    
    uint32_t timestamp;
};

struct BLEVescData {
    float motor_rpm;                // sharedVescData.rpm (eRPM)
    float duty_cycle;               // sharedVescData.duty_cycle
    float temp_mosfet;              // sharedVescData.temp_mosfet
    float temp_motor;               // sharedVescData.temp_motor
    float battery_voltage;          // sharedVescData.battery_voltage
    float battery_percentage;       // sharedVescData.battery_percentage
    float amp_hours;                // sharedVescData.amp_hours
    float watt_hours;               // sharedVescData.watt_hours
    float actual_current;           // sharedVescData.actual_current
    float speed_kmh;                // sharedVescData.speed_kmh
    bool data_valid;                // sharedVescData.data_valid
    uint32_t last_update;           // sharedVescData.last_update
};

// BLE Server callbacks
class EBikeServerCallbacks : public BLEServerCallbacks {
public:
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;
};

// BLE Characteristic callbacks for mode control
class EBikeModeControlCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic* pCharacteristic) override;
};

// BLE Characteristic callbacks for commands
class EBikeCommandCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic* pCharacteristic) override;
};

// BLE task function
void bleTelemetryTask(void *pvParameters);

// Setup function to create BLE task
void setupBLETelemetry();

// BLE control functions
void updateBLETelemetryData();
void updateBLEVescData();
void sendBLEModeList();

// Global declarations for external access
extern TaskHandle_t bleTaskHandle;
extern BLEServer* pBLEServer;
extern bool bleDeviceConnected;
extern bool bleOldDeviceConnected;

// BLE Services
extern BLEService* pTelemetryService;
extern BLEService* pExtendedService;
extern BLEService* pControlService;
extern BLEService* pDeviceInfoService;

// BLE Characteristics (global for access from callbacks)
extern BLECharacteristic* pCharSpeed;
extern BLECharacteristic* pCharCadence;
extern BLECharacteristic* pCharTorque;
extern BLECharacteristic* pCharBattery;
extern BLECharacteristic* pCharCurrent;
extern BLECharacteristic* pCharVescData;
extern BLECharacteristic* pCharSystemStatus;
extern BLECharacteristic* pCharPowerData;
extern BLECharacteristic* pCharTemperatures;
extern BLECharacteristic* pCharCompleteTelemetry;
extern BLECharacteristic* pCharModeControl;
extern BLECharacteristic* pCharModeList;
extern BLECharacteristic* pCharCommand;

#endif // BLE_TELEMETRY_H
