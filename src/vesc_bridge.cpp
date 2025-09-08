/*
  Name:    vesc_bridge.cpp - ESP32 VESC Bridge Mode
  Created: 2025
  Author:  Johannes Lee-Frölich
  
  Description: 
  Transparent bridge mode that allows direct access to VESC controller
  through ESP32's USB Serial connection. This enables remote access to
  VESC Tool when physical access to the controller is not possible.
  
  Features:
  - Bidirectional data forwarding between USB Serial and VESC UART
  - Maintains original VESC communication protocol
  - Compatible with VESC Tool software
  - Minimal overhead for maximum compatibility
  - LED status indication
  
  Based on: https://github.com/A-Emile/VescBLEBridge concept
  
  Hardware:
  - ESP32 DevKit v1
  - VESC connection via Hardware UART2 (GPIO16=RX, GPIO17=TX)
  - Built-in LED for status indication (GPIO2)
  
  Usage:
  1. Set vesc_bridge_mode = true in config.cpp
  2. Upload firmware to ESP32
  3. Connect to ESP32 via USB Serial (115200 baud)
  4. Use VESC Tool with Serial connection pointing to ESP32's COM port
*/

#include <Arduino.h>
#include "ebike_controller.h"

// Bridge configuration
#define BRIDGE_BAUD_RATE        115200    // VESC standard baud rate
#define BUFFER_SIZE             512       // Buffer size for data forwarding
#define BRIDGE_LED_PIN          2         // Built-in LED for status
#define BRIDGE_ACTIVITY_TIMEOUT 100       // Activity LED timeout in ms

// Bridge state variables
static unsigned long last_usb_activity = 0;
static unsigned long last_vesc_activity = 0;
static unsigned long last_led_update = 0;
static bool led_state = false;
static uint32_t bytes_forwarded_usb_to_vesc = 0;
static uint32_t bytes_forwarded_vesc_to_usb = 0;

/**
 * Initialize VESC Bridge Mode
 * Sets up serial connections and status LED
 */
void initVescBridge() {
  // Initialize USB Serial (already done in main setup)
  Serial.begin(BRIDGE_BAUD_RATE);
  
  // Initialize VESC Hardware Serial (UART2)
  Serial2.begin(BRIDGE_BAUD_RATE);
  
  // Initialize status LED
  pinMode(BRIDGE_LED_PIN, OUTPUT);
  digitalWrite(BRIDGE_LED_PIN, LOW);
  
  // Clear any pending data in buffers
  while (Serial.available()) Serial.read();
  while (Serial2.available()) Serial2.read();
  
  Serial.println();
  Serial.println("========================================");
  Serial.println("    ESP32 VESC Bridge Mode Active");
  Serial.println("========================================");
  Serial.println("Bridge Configuration:");
  Serial.println("  - USB Serial:  115200 baud");
  Serial.println("  - VESC UART2:  115200 baud");
  Serial.println("  - Status LED:  GPIO2 (built-in)");
  Serial.println();
  Serial.println("VESC Tool Instructions:");
  Serial.println("1. Open VESC Tool");
  Serial.println("2. Select 'Serial' connection");
  Serial.println("3. Choose this ESP32's COM port");
  Serial.println("4. Set baud rate to 115200");
  Serial.println("5. Connect and configure VESC");
  Serial.println();
  Serial.println("Bridge Status: READY");
  Serial.println("Waiting for VESC Tool connection...");
  Serial.println("========================================");
  
  delay(1000); // Give user time to read startup message
}

/**
 * Update status LED based on bridge activity
 * LED patterns:
 * - Slow blink (1Hz): Bridge ready, no activity
 * - Fast blink (5Hz): Active data transfer
 * - Solid ON: Error or high activity
 */
void updateBridgeStatusLED() {
  unsigned long now = millis();
  bool has_recent_activity = false;
  
  // Check for recent activity (within last 100ms)
  if ((now - last_usb_activity < BRIDGE_ACTIVITY_TIMEOUT) || 
      (now - last_vesc_activity < BRIDGE_ACTIVITY_TIMEOUT)) {
    has_recent_activity = true;
  }
  
  // LED blinking pattern
  unsigned long blink_interval;
  if (has_recent_activity) {
    blink_interval = 100; // Fast blink during activity (5Hz)
  } else {
    blink_interval = 500; // Slow blink when idle (1Hz)
  }
  
  if (now - last_led_update >= blink_interval) {
    led_state = !led_state;
    digitalWrite(BRIDGE_LED_PIN, led_state);
    last_led_update = now;
  }
}

/**
 * Forward data from USB Serial to VESC UART
 * Returns number of bytes forwarded
 */
int forwardUSBToVESC() {
  int bytes_forwarded = 0;
  
  while (Serial.available() && bytes_forwarded < BUFFER_SIZE) {
    uint8_t byte = Serial.read();
    Serial2.write(byte);
    bytes_forwarded++;
    last_usb_activity = millis();
  }
  
  if (bytes_forwarded > 0) {
    bytes_forwarded_usb_to_vesc += bytes_forwarded;
  }
  
  return bytes_forwarded;
}

/**
 * Forward data from VESC UART to USB Serial
 * Returns number of bytes forwarded
 */
int forwardVESCToUSB() {
  int bytes_forwarded = 0;
  
  while (Serial2.available() && bytes_forwarded < BUFFER_SIZE) {
    uint8_t byte = Serial2.read();
    Serial.write(byte);
    bytes_forwarded++;
    last_vesc_activity = millis();
  }
  
  if (bytes_forwarded > 0) {
    bytes_forwarded_vesc_to_usb += bytes_forwarded;
  }
  
  return bytes_forwarded;
}

/**
 * Print bridge statistics (called periodically)
 */
void printBridgeStats() {
  static unsigned long last_stats = 0;
  unsigned long now = millis();
  
  // Print stats every 30 seconds when idle, or every 5 seconds when active
  bool has_recent_activity = (now - last_usb_activity < 5000) || (now - last_vesc_activity < 5000);
  unsigned long stats_interval = has_recent_activity ? 5000 : 30000;
  
  if (now - last_stats >= stats_interval) {
    Serial.println();
    Serial.println("=== VESC Bridge Statistics ===");
    Serial.printf("Uptime: %lu seconds\n", now / 1000);
    Serial.printf("Data forwarded:\n");
    Serial.printf("  USB -> VESC: %u bytes\n", bytes_forwarded_usb_to_vesc);
    Serial.printf("  VESC -> USB: %u bytes\n", bytes_forwarded_vesc_to_usb);
    Serial.printf("Last activity:\n");
    Serial.printf("  USB: %lu ms ago\n", now - last_usb_activity);
    Serial.printf("  VESC: %lu ms ago\n", now - last_vesc_activity);
    Serial.printf("Bridge status: %s\n", has_recent_activity ? "ACTIVE" : "IDLE");
    Serial.println("==============================");
    Serial.println();
    
    last_stats = now;
  }
}

/**
 * Main VESC Bridge Loop
 * Call this continuously when bridge mode is active
 */
void runVescBridge() {
  // Forward data bidirectionally
  int usb_bytes = forwardUSBToVESC();
  int vesc_bytes = forwardVESCToUSB();
  
  // Update status LED
  updateBridgeStatusLED();
  
  // Print statistics periodically
  printBridgeStats();
  
  // Small delay to prevent overwhelming the CPU
  // This is important for stable operation
  delayMicroseconds(100);
}

/**
 * Shutdown bridge mode and clean up resources
 */
void shutdownVescBridge() {
  digitalWrite(BRIDGE_LED_PIN, LOW);
  
  Serial.println();
  Serial.println("========================================");
  Serial.println("    VESC Bridge Mode Shutdown");
  Serial.println("========================================");
  Serial.printf("Final Statistics:\n");
  Serial.printf("  USB -> VESC: %u bytes\n", bytes_forwarded_usb_to_vesc);
  Serial.printf("  VESC -> USB: %u bytes\n", bytes_forwarded_vesc_to_usb);
  Serial.printf("  Total runtime: %lu seconds\n", millis() / 1000);
  Serial.println("Bridge stopped.");
  Serial.println("========================================");
}
