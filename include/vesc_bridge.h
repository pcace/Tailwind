/*
  Name:    vesc_bridge.h - ESP32 VESC Bridge Mode Header
  Created: 2025
  Author:  Johannes Lee-Frölich
  
  Description: 
  Header file for VESC Bridge Mode functionality.
  Provides function declarations for transparent VESC access
  through ESP32's USB Serial connection.
*/

#ifndef VESC_BRIDGE_H
#define VESC_BRIDGE_H

#include <Arduino.h>

// =============================================================================
// VESC BRIDGE MODE FUNCTION DECLARATIONS
// =============================================================================

/**
 * Initialize VESC Bridge Mode
 * Sets up serial connections and status LED
 */
void initVescBridge();

/**
 * Main VESC Bridge Loop
 * Call this continuously when bridge mode is active
 */
void runVescBridge();

/**
 * Update status LED based on bridge activity
 */
void updateBridgeStatusLED();

/**
 * Forward data from USB Serial to VESC UART
 * Returns number of bytes forwarded
 */
int forwardUSBToVESC();

/**
 * Forward data from VESC UART to USB Serial
 * Returns number of bytes forwarded
 */
int forwardVESCToUSB();

/**
 * Print bridge statistics (called periodically)
 */
void printBridgeStats();

/**
 * Shutdown bridge mode and clean up resources
 */
void shutdownVescBridge();

#endif // VESC_BRIDGE_H
