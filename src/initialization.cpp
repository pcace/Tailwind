#include "ebike_controller.h"
#include <VescUart.h>

// External VESC UART instance (defined in main.cpp)
extern VescUart vescUart;

// =============================================================================
// STARTUP LED SEQUENCE
// =============================================================================

void startup_battery_indicator() {
  // Turn on LED for 1 second to show controller is alive
  digitalWrite(BATTERY_LED_PIN, HIGH);
  delay(1000);
  digitalWrite(BATTERY_LED_PIN, LOW);
  delay(200);
  
  // Try to get battery data from VESC (with timeout)
  unsigned long vesc_timeout = millis();
  bool battery_data_available = false;
  
  Serial.println("Reading battery status for startup indicator...");
  
  // Try to get VESC data for up to 10 seconds
  while (millis() - vesc_timeout < 10000) {
    if (vescUart.getVescValues()) {
      battery_voltage = vescUart.data.inpVoltage;
      
      // Calculate battery percentage
      if (battery_voltage > BATTERY_FULL_VOLTAGE) {
        battery_percentage = 100.0;
      } else if (battery_voltage < BATTERY_CRITICAL_VOLTAGE) {
        battery_percentage = 0.0;
      } else {
        battery_percentage = ((battery_voltage - BATTERY_CRITICAL_VOLTAGE) / 
                             (BATTERY_FULL_VOLTAGE - BATTERY_CRITICAL_VOLTAGE)) * 100.0;
      }
      
      battery_data_available = true;
      break;
    }
    delay(100);
  }
  
  if (battery_data_available) {
    // Calculate number of blinks (each 10% = 1 blink)
    int blinks = (int)(battery_percentage / 10.0);
    
    Serial.printf("Battery: %.1fV (%.0f%%) - Showing %d blinks\n", 
                  battery_voltage, battery_percentage, blinks);
    
    // Show battery level through LED blinks
    for (int i = 0; i < blinks; i++) {
      digitalWrite(BATTERY_LED_PIN, HIGH);
      delay(200);  // 0.2s on
      digitalWrite(BATTERY_LED_PIN, LOW);
      delay(200);  // 0.2s off
    }
  } else {
    Serial.println("Could not read battery data - skipping battery indicator");
    // Flash LED 3 times quickly to indicate error
    for (int i = 0; i < 3; i++) {
      digitalWrite(BATTERY_LED_PIN, HIGH);
      delay(100);
      digitalWrite(BATTERY_LED_PIN, LOW);
      delay(100);
    }
  }
  
  // Ensure LED is off after sequence
  digitalWrite(BATTERY_LED_PIN, LOW);
}

// =============================================================================
// INITIALIZATION
// =============================================================================

void ebike_setup() {
  // Initialize assist profiles from configuration
  initializeAssistProfiles();
  
  // Pin configurations
  pinMode(LIGHT_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, LOW);
  
  // Battery status LED pin
  pinMode(BATTERY_LED_PIN, OUTPUT);
  digitalWrite(BATTERY_LED_PIN, LOW);
  
  // PAS sensor pins as input with pull-up
  pinMode(PAS_PIN_A, INPUT_PULLUP);
  pinMode(PAS_PIN_B, INPUT_PULLUP);
  
  // Enable hardware interrupts for PAS sensors
  attachInterrupt(digitalPinToInterrupt(PAS_PIN_A), pas_interrupt_handler, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PAS_PIN_B), pas_interrupt_handler, CHANGE);
  
  // Show startup battery indicator
  startup_battery_indicator();
  
  // *** TORQUE SENSOR CALIBRATION ***
  // Calibrate torque sensor neutral position on every startup
  // This eliminates drift and ensures accurate zero-point detection
  Serial.println("=== STARTING TORQUE SENSOR CALIBRATION ===");
  calibrate_torque_sensor();
  
  // Set initial values
  last_loop_time = millis();
  last_pedal_activity = millis();
  
  Serial.println("=== E-Bike Controller v2.0 ===");
  Serial.println("Torque+PAS+Speed combination");
  Serial.print("Active profiles: ");
  Serial.println(NUM_ACTIVE_PROFILES);
  Serial.println("System ready!");
}
