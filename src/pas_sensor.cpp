#include "ebike_controller.h"
#include <limits.h>

// ESP32 FreeRTOS Includes
#ifdef ESP32
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "esp32-hal.h"  // Für ESP32-spezifische Funktionen
#endif

// =============================================================================
// INTERRUPT HANDLER FOR PAS SENSORS - Multi-Core Optimized
// =============================================================================

void IRAM_ATTR pas_interrupt_handler() {
  // Interrupt Service Routine - must be very fast!
  // IRAM_ATTR ensures this function runs from RAM for maximum speed
  static unsigned long last_time = 0;
  unsigned long now = micros();  // micros() is safer in ISR than millis()
  
  // Debounce: Minimum 1500 microseconds (1.5ms) between interrupts
  // Reduced from 2ms for better high-cadence response
  if (now - last_time < 1500) {
    return;
  }
  
  last_time = now;
  last_interrupt_time = now;  // Store for main loop
  pas_interrupt_flag = true;  // Set flag for main loop
}

// =============================================================================
// CADENCE MEASUREMENT (PAS sensor evaluation) - Enhanced
// =============================================================================

void update_cadence() {
  // DEBUG MODE: Use simulated values instead of sensor data
  if (debug_mode && debug_simulate_pas) {
    update_debug_simulation(); // Update debug simulation values
    
    current_cadence_rpm = debug_cadence_rpm;
    current_cadence_rps = debug_cadence_rpm / 60.0;
    
    // Simulate pedal direction based on cadence
    if (debug_cadence_rpm > 5.0) {
      pedal_direction = 1;  // Forward
      last_pedal_activity = millis();
    } else {
      pedal_direction = 0;  // Standstill
    }
    
    return; // Skip real sensor processing
  }
  
  // NORMAL MODE: Original sensor processing
  unsigned long now = millis();
  
  // Timeout check: If too long without pedals, set cadence to 0
  if (now - last_pulse_time > CADENCE_WINDOW_MS) {
    current_cadence_rpm = 0.0;
    current_cadence_rps = 0.0;
    pedal_direction = 0;  // Standstill
    pos = 0;  // Reset position on standstill
    
    // Reset interrupt timing as well
    pas_interrupt_flag = false;
    last_interrupt_time = 0;
    return;
  }
  
  // Additional smoothing: Gradual decay if no recent activity
  if (now - last_pulse_time > CADENCE_WINDOW_MS / 2) {
    // Gradually reduce cadence if no recent pulses
    current_cadence_rpm *= 0.95;  // 5% decay per call
    if (current_cadence_rpm < 1.0) {
      current_cadence_rpm = 0.0;
    }
    current_cadence_rps = current_cadence_rpm / 60.0;
  }
}

// =============================================================================
// PAS HALL SENSOR EVALUATION - Multi-Core Safe
// =============================================================================

void read_pas_sensors() {
  // DEBUG MODE: Skip real sensor reading
  if (debug_mode && debug_simulate_pas) {
    return; // All simulation is handled in update_cadence()
  }
  
  // NORMAL MODE: Original sensor processing
  // Check for interrupt flag first - this is set by hardware interrupt
  if (!pas_interrupt_flag) {
    return;  // No new interrupt - nothing to do
  }
  
  // Critical section: disable interrupts briefly to read sensor state
  // Verwende Arduino-kompatible Interrupt-Kontrolle
  noInterrupts();
  pas_interrupt_flag = false;  // Reset flag
  unsigned long interrupt_time = last_interrupt_time;
  interrupts();
  
  // Read current sensor states (digital pins)
  int newA = digitalRead(PAS_PIN_A);
  int newB = digitalRead(PAS_PIN_B);
  
  // Check if something really changed
  if (newA == a && newB == b) {
    return;  // False alarm, no change
  }
  
  unsigned long now = millis();
  
  // Quadrature decoding for direction and position
  int old_state = (a << 1) | b;       // Old state: A*2 + B
  int new_state = (newA << 1) | newB; // New state: A*2 + B
  
  // Quadrature lookup table for direction:
  // [old_state][new_state] = direction (1=forward, -1=backward, 0=invalid)
  static const int quadrature_table[4][4] = {
    // new: 00  01  11  10
    {  0,  1, -1,  0}, // old: 00
    { -1,  0,  0,  1}, // old: 01  
    {  1,  0,  0, -1}, // old: 11
    {  0, -1,  1,  0}  // old: 10
  };
  
  int direction_change = quadrature_table[old_state][new_state];
  
  if (direction_change != 0) {
    // Valid state transition
    pos += direction_change;
    pedal_direction = direction_change;  // 1=forward, -1=backward
    
    // ENHANCED CONTINUOUS CADENCE CALCULATION at every step
    if (pedal_direction > 0 && last_pulse_time > 0) {  // Only during forward movement
      unsigned long step_interval = now - last_pulse_time;
        
      // Wider plausible range for better responsiveness
      if (step_interval > 5 && step_interval < 3000) {  // 5ms - 3s
        // Calculate RPM based on current step speed
        // One step = 1/32 revolution (quadrature encoding: 8 pulses * 4 edges = 32)
        unsigned long revolution_time = step_interval * quadrature_pulses_per_rev;
        
        // Calculate raw cadence
        float raw_cadence_rpm = 60000.0 / revolution_time;
        
        // Enhanced plausibility check (3-200 RPM for better range)
        if (raw_cadence_rpm >= 3.0 && raw_cadence_rpm <= 200.0) {
          // More responsive smoothing for multi-core environment
          float alpha = 0.4;  // Higher alpha = more responsive
          
          if (current_cadence_rpm > 0.0) {
            // Adaptive smoothing: more responsive when cadence is changing rapidly
            float cadence_change_rate = abs(raw_cadence_rpm - current_cadence_rpm) / current_cadence_rpm;
            if (cadence_change_rate > 0.2) {  // >20% change
              alpha = 0.6;  // More responsive during rapid changes
            }
            
            current_cadence_rpm = current_cadence_rpm * (1.0 - alpha) + raw_cadence_rpm * alpha;
          } else {
            current_cadence_rpm = raw_cadence_rpm;  // Take first value immediately
          }
          current_cadence_rps = current_cadence_rpm / 60.0;
        }
      }
    }
    
    // Legacy pulse interval for compatibility (ring buffer)
    if (last_pulse_time > 0) {
      unsigned long interval = now - last_pulse_time;
      pulse_intervals[pulse_index] = interval;
      pulse_index = (pulse_index + 1) % 4;  // Ring buffer with 4 entries
    }
    
    last_pulse_time = now;
    last_pedal_activity = now;
  }
  
  // Store states for next iteration
  a = newA;
  b = newB;
  
  // Overflow protection with better handling
  if (pos >= INT_MAX - 1000 || pos <= INT_MIN + 1000) {
    int old_pos = pos;
    pos = pos % 10000;  // Keep relative position but reset to manageable range
    vescCounter += (pos - old_pos);  // Maintain vescCounter continuity
    
    Serial.printf("Position overflow protection: %d -> %d\n", old_pos, pos);
  }
}
