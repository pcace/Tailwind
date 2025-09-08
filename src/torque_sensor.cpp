#include "ebike_controller.h"

// =============================================================================
// TORQUE SENSOR CALIBRATION
// =============================================================================

void calibrate_torque_sensor() {
  // Skip calibration in debug mode
  if (debug_mode) {
    Serial.println("TORQUE CALIBRATION: Skipped - Debug mode active");
    torque_standstill_calibrated = TORQUE_STANDSTILL_DEFAULT;
    torque_calibration_complete = true;
    return;
  }
  
  Serial.println("=== TORQUE SENSOR CALIBRATION STARTING ===");
  Serial.println("Please ensure NO FORCE is applied to the pedals!");
  Serial.println("Calibration will start in 2 seconds...");
  
  delay(2000);  // Give user time to read message
  
  Serial.printf("Taking %d calibration samples...\n", TORQUE_CALIBRATION_SAMPLES);
  
  long total_readings = 0;
  int valid_samples = 0;
  unsigned long start_time = millis();
  
  for (int i = 0; i < TORQUE_CALIBRATION_SAMPLES; i++) {
    // Check for timeout
    if (millis() - start_time > TORQUE_CALIBRATION_TIMEOUT_MS) {
      Serial.println("TORQUE CALIBRATION: Timeout reached!");
      break;
    }
    
    // Read ADC value
    int reading = analogRead(TORQUE_SENSOR_PIN);
    
    // Sanity check: ADC reading should be within reasonable range
    if (reading >= 100 && reading <= 3995) {  // Allow 100-3995 for 12-bit ADC (avoid extremes)
      total_readings += reading;
      valid_samples++;
      
      Serial.printf("Sample %d/%d: %d ADC\n", i + 1, TORQUE_CALIBRATION_SAMPLES, reading);
    } else {
      Serial.printf("Sample %d/%d: %d ADC (INVALID - out of range)\n", i + 1, TORQUE_CALIBRATION_SAMPLES, reading);
    }
    
    delay(TORQUE_CALIBRATION_DELAY_MS);
  }
  
  // Calculate calibrated value
  if (valid_samples >= (TORQUE_CALIBRATION_SAMPLES / 2)) {  // Need at least 50% valid samples
    torque_standstill_calibrated = (int)(total_readings / valid_samples);
    torque_calibration_complete = true;
    
    Serial.printf("=== TORQUE CALIBRATION COMPLETED ===\n");
    Serial.printf("Valid samples: %d/%d\n", valid_samples, TORQUE_CALIBRATION_SAMPLES);
    Serial.printf("Default standstill: %d ADC\n", TORQUE_STANDSTILL_DEFAULT);
    Serial.printf("Calibrated standstill: %d ADC\n", torque_standstill_calibrated);
    Serial.printf("Drift compensation: %d ADC\n", torque_standstill_calibrated - TORQUE_STANDSTILL_DEFAULT);
    Serial.println("========================================");
  } else {
    Serial.printf("TORQUE CALIBRATION: FAILED - Only %d/%d valid samples\n", valid_samples, TORQUE_CALIBRATION_SAMPLES);
    Serial.printf("Using default value: %d ADC\n", TORQUE_STANDSTILL_DEFAULT);
    
    // Fall back to default value
    torque_standstill_calibrated = TORQUE_STANDSTILL_DEFAULT;
    torque_calibration_complete = true;
  }
}

bool is_torque_calibration_complete() {
  return torque_calibration_complete;
}

// =============================================================================
// TORQUE SENSOR EVALUATION (Absolute torque from calibrated center point)
// =============================================================================

void update_torque() {
  // DEBUG MODE: Use simulated values instead of sensor data
  if (debug_mode && debug_simulate_torque) {
    update_debug_simulation(); // Update debug simulation values
    
    crank_torque_nm = debug_torque_nm;
    filtered_torque = debug_torque_nm; // Direct assignment in debug mode
    
    // Simulate raw ADC value for debugging purposes
    // Scale torque back to ADC range for consistency (ESP32: 0-4095)
    if (debug_torque_nm > 0.0) {
      float torque_ratio = debug_torque_nm / TORQUE_MAX_NM;
      int max_deviation = max(torque_standstill_calibrated - TORQUE_MAX_BACKWARD, 
                             TORQUE_MAX_FORWARD - torque_standstill_calibrated);
      int simulated_deviation = (int)(torque_ratio * max_deviation);
      raw_torque_value = torque_standstill_calibrated + simulated_deviation;
    } else {
      raw_torque_value = torque_standstill_calibrated;
    }
    
    return; // Skip real sensor processing
  }
  
  // NORMAL MODE: Original sensor processing
  // Read ADC value (0-4095 for 12-bit ADC on ESP32)
  // ESP32 ADC measures 0-3.3V with 12-bit resolution
  // Torque sensor with 3kΩ pull-down resistor for voltage divider
  raw_torque_value = analogRead(TORQUE_SENSOR_PIN);
  
  // Calculate deviation from calibrated center point 
  // Use the dynamically calibrated standstill value instead of fixed constant
  int deviation_from_center = raw_torque_value - torque_standstill_calibrated;
  
  // Use ABSOLUTE VALUE - force intensity regardless of pedal position
  int absolute_deviation = abs(deviation_from_center);
  
  // Check if deviation is above threshold
  if (absolute_deviation < TORQUE_THRESHOLD) {
    crank_torque_nm = 0.0;  // Below threshold = no meaningful torque
  } else {
    // Calculate maximum possible deviation from calibrated center
    // Use calibrated standstill value for accurate calculation
    int max_deviation = max(torque_standstill_calibrated - TORQUE_MAX_BACKWARD, 
                           TORQUE_MAX_FORWARD - torque_standstill_calibrated);
    
    // Scale absolute deviation to 0-TORQUE_MAX_NM range
    crank_torque_nm = (float)absolute_deviation / max_deviation * TORQUE_MAX_NM;
  }
  
  // Clamp to reasonable range (only positive values)
  if (crank_torque_nm > TORQUE_MAX_NM) crank_torque_nm = TORQUE_MAX_NM;
  if (crank_torque_nm < 0.0) crank_torque_nm = 0.0;
  
  // Direct assignment without filtering
  filtered_torque = crank_torque_nm;
}
