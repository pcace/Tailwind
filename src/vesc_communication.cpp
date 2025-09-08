#include "ebike_controller.h"
#include <VescUart.h>

// WiFi Logging Integration
#include "wifi_telemetry.h"

// ESP32 FreeRTOS Includes für Task-Funktionen
#ifdef ESP32
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
#endif

// External VESC UART instance (created in main.cpp)
extern VescUart vescUart;

// =============================================================================
// VESC DATA QUERY (Speed) - Optimized for Multi-Core
// =============================================================================

void update_vesc_data() {
  unsigned long now = millis();
  
  // Query VESC data at lower frequency since it runs on separate core
  // and doesn't interfere with sensor processing anymore
  static unsigned long last_vesc_query = 0;
  if (now - last_vesc_query < 100) {  // 100ms = 10Hz query rate
    return;
  }
  
  last_vesc_query = now;
  
  // VESC query with timeout - CAN BLOCK without affecting Core 0!
  unsigned long vesc_start_time = millis();
  bool vesc_success = false;
  
  // Try to query VESC data with reasonable timeout 
  // Since we're on Core 1, blocking here doesn't affect PAS sensors
  while (millis() - vesc_start_time < 100) {  // Max 100ms timeout
    if (vescUart.getVescValues()) {
      vesc_success = true;
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));  // Use FreeRTOS delay instead of Arduino delay
  }
  
  if (vesc_success) {
    // Successful data query
    vesc_data_valid = true;
    last_vesc_data_time = now;
    
    // Calculate speed from eRPM (CORRECTED FOR Q100C NABENMOTOR!)
    float erpm = vescUart.data.rpm;
    float pole_pairs = MOTOR_POLES / 2.0;           // 16 poles = 8 pole pairs
    
    // Q100C is a geared hub motor: eRPM → rotor RPM → wheel RPM → speed
    // The gear ratio is INSIDE the motor hub, reducing rotor speed to wheel speed
    float rotor_rpm = erpm / pole_pairs;           // Rotor RPM (high speed)
    current_motor_rpm = rotor_rpm;                 // Store rotor RPM for power calculation
    float wheel_rpm = rotor_rpm / MOTOR_GEAR_RATIO; // Wheel RPM (low speed, after gear reduction)
    float wheel_circumference_m = PI * WHEEL_DIAMETER_M;
    
    // km/h = (wheel_revolutions/min) × (circumference in m) × (60 min/h) × (1 km/1000m)
    current_speed_kmh = wheel_rpm * wheel_circumference_m * 0.06; // 60/1000
    
    // Plausibility check (E-bikes don't go over 50 km/h)
    if (current_speed_kmh < 0 || current_speed_kmh > 50.0) {
      current_speed_kmh = 0.0;
      vesc_data_valid = false;
    }
    
    // Also read actual current for monitoring
    actual_current_amps = vescUart.data.avgMotorCurrent;
    
    // Extended VESC data for web interface
    float erpm_raw = vescUart.data.rpm;
    float duty_cycle_raw = vescUart.data.dutyCycleNow;
    float temp_mosfet_raw = vescUart.data.tempMosfet;
    float temp_motor_raw = vescUart.data.tempMotor;
    float amp_hours_raw = vescUart.data.ampHours;
    float watt_hours_raw = vescUart.data.wattHours;
    
    // Update shared VESC data (thread-safe)
    if (xSemaphoreTake(dataUpdateSemaphore, pdMS_TO_TICKS(5)) == pdTRUE) {
      sharedVescData.speed_kmh = current_speed_kmh;
      sharedVescData.data_valid = vesc_data_valid;
      sharedVescData.actual_current = actual_current_amps;
      sharedVescData.battery_voltage = battery_voltage;
      sharedVescData.battery_percentage = battery_percentage;
      
      // Extended data
      sharedVescData.rpm = erpm_raw;
      sharedVescData.duty_cycle = duty_cycle_raw;
      sharedVescData.temp_mosfet = temp_mosfet_raw;
      sharedVescData.temp_motor = temp_motor_raw;
      sharedVescData.amp_hours = amp_hours_raw;
      sharedVescData.watt_hours = watt_hours_raw;
      sharedVescData.last_update = now;
      
      xSemaphoreGive(dataUpdateSemaphore);
    }
    
    // Read battery voltage and calculate percentage
    battery_voltage = vescUart.data.inpVoltage;
    
    // Calculate battery percentage (linear approximation)
    // For 48V system: Full=54.6V (13S * 4.2V), Empty=40.8V (13S * 3.1V)
    if (battery_voltage > BATTERY_FULL_VOLTAGE) {
      battery_percentage = 100.0;
    } else if (battery_voltage < BATTERY_CRITICAL_VOLTAGE) {
      battery_percentage = 0.0;
    } else {
      battery_percentage = ((battery_voltage - BATTERY_CRITICAL_VOLTAGE) / 
                           (BATTERY_FULL_VOLTAGE - BATTERY_CRITICAL_VOLTAGE)) * 100.0;
    }
    
    // Update battery status
    update_battery_status();
    
  } else {
    // VESC communication failed or timeout
    vesc_data_valid = false;
    current_speed_kmh = 0.0;
    
    // Connection lost handling
    static unsigned long connection_lost_time = 0;
    if (connection_lost_time == 0) {
      connection_lost_time = now;
      Serial.println("WARNING: VESC connection lost!");
      addLogMessage("WARNING: VESC connection lost!");
    }
    
    // After 5 seconds without connection, go to safe mode
    if (now - connection_lost_time > 5000) {
      motor_enabled = false;
      addLogMessage("SAFETY: Motor disabled - VESC connection failed");
      // Serial.println("SAFETY: Motor disabled due to VESC connection loss");
    }
  }
}

// =============================================================================
// BATTERY MONITORING FUNCTIONS
// =============================================================================

void update_battery_status() {
  // Check if battery is critical (≤10%)
  if (battery_percentage <= BATTERY_CRITICAL_THRESHOLD) {
    if (!battery_critical) {
      battery_critical = true;
      battery_low = true;  // Critical implies low
      Serial.printf("CRITICAL: Battery critically low! Voltage: %.1fV (%.1f%%) - Fast blinking!\n", 
                   battery_voltage, battery_percentage);
      addLogMessage("CRITICAL: Battery critically low! " + String(battery_percentage, 0) + "% (" + String(battery_voltage, 1) + "V)");
    }
  } else if (battery_percentage <= BATTERY_LOW_THRESHOLD) {
    // Check if battery is low (≤20%)
    if (!battery_low) {
      battery_low = true;
      battery_critical = false;
      Serial.printf("WARNING: Low battery! Voltage: %.1fV (%.1f%%)\n", 
                   battery_voltage, battery_percentage);
      addLogMessage("WARNING: Battery low! " + String(battery_percentage, 0) + "% (" + String(battery_voltage, 1) + "V)");
    } else if (battery_critical) {
      // Battery recovered from critical to low
      battery_critical = false;
      Serial.printf("INFO: Battery recovered from critical to low. Voltage: %.1fV (%.1f%%)\n", 
                   battery_voltage, battery_percentage);
      addLogMessage("INFO: Battery recovered from critical to low. " + String(battery_percentage, 0) + "% (" + String(battery_voltage, 1) + "V)");
    }
  } else {
    // Battery is OK
    if (battery_low || battery_critical) {
      battery_low = false;
      battery_critical = false;
      Serial.printf("INFO: Battery level OK again. Voltage: %.1fV (%.1f%%)\n", 
                   battery_voltage, battery_percentage);
    }
  }
  
  // Update battery LED
  update_battery_led();
}

void update_battery_led() {
  unsigned long now = millis();
  
  if (battery_low) {
    // Choose blink interval based on battery status
    unsigned long blink_interval;
    if (battery_critical) {
      blink_interval = BATTERY_LED_FAST_BLINK_INTERVAL;  // Fast blink for ≤10%
    } else {
      blink_interval = BATTERY_LED_BLINK_INTERVAL;       // Normal blink for ≤20%
    }
    
    // Blink LED when battery is low or critical
    if (now - last_battery_led_toggle >= blink_interval) {
      battery_led_state = !battery_led_state;
      digitalWrite(BATTERY_LED_PIN, battery_led_state ? HIGH : LOW);
      last_battery_led_toggle = now;
    }
  } else {
    // Turn LED off when battery is OK
    if (battery_led_state) {
      battery_led_state = false;
      digitalWrite(BATTERY_LED_PIN, LOW);
    }
  }
}
