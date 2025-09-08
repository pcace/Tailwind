#include "ebike_controller.h"
#include <VescUart.h>

// WiFi Logging Integration
#include "wifi_telemetry.h"

// ESP32 FreeRTOS Includes für Semaphore-Funktionen
#ifdef ESP32
  #include "freertos/FreeRTOS.h"
  #include "freertos/semphr.h"
#endif

// External VESC UART instance (created in main.cpp)
extern VescUart vescUart;

// =============================================================================
// MOTOR STATUS - Enhanced for Multi-Core
// =============================================================================

void update_motor_status() {
  unsigned long now = millis();
  
  vescDelayBetween++;
  if (vescCounter < pos || vescDelayBetween > 900) {
    vescCounter = pos;
    vescDelayBetweenList = vescDelayBetween;
    vescDelayBetween = 0;
  }
  
  // Motor activation based on multiple criteria:
  bool pas_active = (now - last_pedal_activity) < PEDAL_TIMEOUT_MS;
  bool torque_present = abs(filtered_torque) > 0.1;  // REDUCED: Minimum torque 0.5 Nm for better start-up
  bool cadence_valid = current_cadence_rpm > 1.0; // REDUCED: Minimum cadence 2 RPM for better start-up
  bool mode_allows_assist = (current_mode >= 0 && current_mode < NUM_ACTIVE_PROFILES); // FIXED: Include mode 0
  bool forward_pedaling = pedal_direction > 0; // Only forward pedaling
  
  // Additional safety: Check VESC data freshness in multi-core environment
  bool vesc_data_fresh = true;
  if (xSemaphoreTake(dataUpdateSemaphore, pdMS_TO_TICKS(1)) == pdTRUE) {
    vesc_data_fresh = (now - sharedVescData.last_update) < 1000; // VESC data less than 1s old
    xSemaphoreGive(dataUpdateSemaphore);
  }
  
  // DEBUG: Log all conditions periodically
  static unsigned long last_motor_debug = 0;
  if (now - last_motor_debug > 1000) { // Every 1 second
    Serial.printf("MOTOR DEBUG - PAS:%s Torque:%s(%.1f) Cadence:%s(%.1f) Mode:%s(%d) Dir:%s VescFresh:%s\n", 
                  pas_active ? "OK" : "NO", 
                  torque_present ? "OK" : "NO", filtered_torque,
                  cadence_valid ? "OK" : "NO", current_cadence_rpm,
                  mode_allows_assist ? "OK" : "NO", current_mode,
                  forward_pedaling ? "FWD" : "STOP",
                  vesc_data_fresh ? "YES" : "NO");
    last_motor_debug = now;
  }
  
  motor_enabled = pas_active && torque_present && cadence_valid && 
                 mode_allows_assist && forward_pedaling && vesc_data_fresh;
  
  // Additional safety checks
  if (current_cadence_rpm > 250.0) {  // Over 250 RPM = unrealistic
    motor_enabled = false;
    Serial.println("MOTOR: Disabled due to excessive cadence");
    addLogMessage("WARNING: Motor stopped - excessive cadence (" + String(current_cadence_rpm) + " RPM)");
  }
  
  if (abs(raw_torque_value - torque_standstill_calibrated) < TORQUE_THRESHOLD) {  // No torque detected
    motor_enabled = false;
    // if (now - last_motor_debug < 100) { // Only log occasionally
    // Serial.printf("MOTOR: Disabled - torque below threshold (Raw: %d, Standstill: %d, Threshold: %d)\n", 
    //  raw_torque_value, torque_standstill_calibrated, TORQUE_THRESHOLD);
    // }

  }

  // Emergency stop on excessive speed
  if (current_speed_kmh > 45.0) {
    motor_enabled = false;
    target_current_amps = 0.0;
    Serial.println("EMERGENCY: Speed limit exceeded - motor disabled!");
    addLogMessage("EMERGENCY: Speed limit exceeded (" + String(current_speed_kmh) + " km/h) - motor stopped!");
  }

}

// =============================================================================
// VESC MOTOR COMMAND - Thread-Safe
// =============================================================================

void send_motor_command() {
  // This function is now called from the VESC task (Core 1)
  // It reads the motor command from shared data and sends it to VESC
  
  static float last_sent_current = -1.0;
  static unsigned long last_vesc_debug = 0;
  float current_to_send = 0.0;
  
  if (motor_enabled) {
    current_to_send = target_current_amps;
  } else {
    current_to_send = 0.0;
  }
  
  // DEBUG: Always log VESC commands for debugging
  unsigned long now = millis();
  if (now - last_vesc_debug > 500) { // Every 500ms
    Serial.printf("VESC SEND - Motor:%s Target:%.2fA Current:%.2fA Human:%.0fW Assist:%.0fW Factor:%.2f\n", 
                  motor_enabled ? "ON" : "OFF", 
                  target_current_amps, current_to_send,
                  human_power_watts, assist_power_watts, dynamic_assist_factor);
    last_vesc_debug = now;
  }
  
  // Only send command if current changed significantly (reduces UART traffic)
  if (abs(current_to_send - last_sent_current) > 0.1 || current_to_send == 0.0) {
    vescUart.setCurrent(current_to_send);
    last_sent_current = current_to_send;
    
    // Debug output for any command change
    Serial.printf("VESC CMD SENT: %.2fA (Motor:%s, Mode:%d, Cadence:%.1f, Torque:%.1f)\n", 
                 current_to_send, motor_enabled ? "ON" : "OFF", 
                 current_mode, current_cadence_rpm, filtered_torque);
  }
}
