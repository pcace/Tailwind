#include "ebike_controller.h"

// =============================================================================
// DEBUG SIMULATION - Simuliert realistische PAS und Torque Werte
// =============================================================================

// Test arrays for systematic testing
static const float TEST_CADENCES[DEBUG_TEST_CADENCE_COUNT] = DEBUG_TEST_CADENCES;
static const float TEST_TORQUES[DEBUG_TEST_TORQUE_COUNT] = DEBUG_TEST_TORQUES;

void update_debug_simulation() {
  if (!debug_mode) {
    return; // Debug mode nicht aktiv
  }
  
  unsigned long now = millis();
  
  // Update nur alle DEBUG_UPDATE_INTERVAL_MS durchführen
  if (now - debug_last_update < DEBUG_UPDATE_INTERVAL_MS) {
    return;
  }
  
  debug_last_update = now;
  
  // Wähle Debug-Modus
  if (debug_simulation_mode == DEBUG_MODE_SYSTEMATIC_TEST) {
    update_systematic_test_simulation(now);
  } else {
    update_smooth_cycle_simulation(now);
  }
}

// =============================================================================
// SYSTEMATIC TEST SIMULATION
// =============================================================================

void update_systematic_test_simulation(unsigned long now) {
  // Initialisierung beim ersten Aufruf
  if (debug_test_start_time == 0) {
    debug_test_start_time = now;
    debug_test_cadence_index = 0;
    debug_test_torque_index = 0;
    debug_test_completed = false;
    
    Serial.println("=== SYSTEMATIC DEBUG TEST STARTED ===");
    Serial.printf("Testing %d cadence values x %d torque values = %d combinations\n", 
                  DEBUG_TEST_CADENCE_COUNT, DEBUG_TEST_TORQUE_COUNT, 
                  DEBUG_TEST_CADENCE_COUNT * DEBUG_TEST_TORQUE_COUNT);
    Serial.printf("Each combination tested for %d seconds\n", DEBUG_TEST_DURATION_MS / 1000);
  }
  
  // Prüfe ob Test abgeschlossen
  if (debug_test_completed) {
    debug_cadence_rpm = 0.0;
    debug_torque_nm = 0.0;
    
    static unsigned long last_completion_msg = 0;
    if (now - last_completion_msg > 10000) { // Alle 10 Sekunden
      Serial.println("=== ALL SYSTEMATIC TESTS COMPLETED ===");
      Serial.println("Change debug_simulation_mode to restart or switch modes");
      last_completion_msg = now;
    }
    return;
  }
  
  // Prüfe ob es Zeit ist, zum nächsten Test zu wechseln
  if (now - debug_test_start_time >= DEBUG_TEST_DURATION_MS) {
    // Zum nächsten Drehmoment
    debug_test_torque_index++;
    
    if (debug_test_torque_index >= DEBUG_TEST_TORQUE_COUNT) {
      // Alle Drehmomente für diese Cadence getestet -> nächste Cadence
      debug_test_torque_index = 0;
      debug_test_cadence_index++;
      
      if (debug_test_cadence_index >= DEBUG_TEST_CADENCE_COUNT) {
        // Alle Tests abgeschlossen
        debug_test_completed = true;
        Serial.println("=== SYSTEMATIC TEST SEQUENCE COMPLETED ===");
        return;
      }
    }
    
    // Neue Test-Zeit setzen
    debug_test_start_time = now;
    
    // Neue Werte loggen
    Serial.printf("SYSTEMATIC TEST - Cadence %d/%d: %.0f RPM, Torque %d/%d: %.0f Nm\n",
                  debug_test_cadence_index + 1, DEBUG_TEST_CADENCE_COUNT, TEST_CADENCES[debug_test_cadence_index],
                  debug_test_torque_index + 1, DEBUG_TEST_TORQUE_COUNT, TEST_TORQUES[debug_test_torque_index]);
  }
  
  // Aktuelle Werte setzen
  if (debug_simulate_pas) {
    debug_cadence_rpm = TEST_CADENCES[debug_test_cadence_index];
  }
  
  if (debug_simulate_torque) {
    debug_torque_nm = TEST_TORQUES[debug_test_torque_index];
  }
  
  // Fortschritts-Info alle 2 Sekunden
  static unsigned long last_progress_log = 0;
  if (now - last_progress_log > 2000) {
    unsigned long time_in_test = now - debug_test_start_time;
    unsigned long remaining_time = DEBUG_TEST_DURATION_MS - time_in_test;
    
    int total_tests = DEBUG_TEST_CADENCE_COUNT * DEBUG_TEST_TORQUE_COUNT;
    int completed_tests = debug_test_cadence_index * DEBUG_TEST_TORQUE_COUNT + debug_test_torque_index;
    
    Serial.printf("TEST PROGRESS - %d/%d (%.1f%%) - Current: %.0fRPM/%.0fNm - Remaining: %.1fs\n",
                  completed_tests + 1, total_tests, 
                  ((float)(completed_tests + 1) / total_tests) * 100.0,
                  debug_cadence_rpm, debug_torque_nm,
                  remaining_time / 1000.0);
    
    last_progress_log = now;
  }
}

// =============================================================================
// SMOOTH CYCLE SIMULATION (Original)
// =============================================================================

void update_smooth_cycle_simulation(unsigned long now) {
  // Berechne Position im Zyklus (0.0 bis 1.0)
  unsigned long cycle_time = now % DEBUG_CYCLE_DURATION_MS;
  float cycle_position = (float)cycle_time / DEBUG_CYCLE_DURATION_MS;
  
  // Debug-Zyklus: 4 Phasen à 25% der Zeit
  // Phase 0 (0-25%): Ramp up - Langsam hochfahren
  // Phase 1 (25-50%): Hold high - Hohe Werte halten
  // Phase 2 (50-75%): Ramp down - Langsam runterfahren  
  // Phase 3 (75-100%): Hold low - Niedrige Werte halten
  
  if (cycle_position < 0.25) {
    // Phase 0: Ramp up
    debug_cycle_state = 0;
    float ramp_progress = cycle_position / 0.25; // 0.0 bis 1.0
    
    if (debug_simulate_pas) {
      debug_cadence_rpm = ramp_progress * DEBUG_MAX_CADENCE;
    }
    
    if (debug_simulate_torque) {
      debug_torque_nm = ramp_progress * DEBUG_MAX_TORQUE;
    }
    
  } else if (cycle_position < 0.50) {
    // Phase 1: Hold high
    debug_cycle_state = 1;
    
    if (debug_simulate_pas) {
      // Kleine Variation um maximale Werte
      float variation = sin((cycle_position - 0.25) * 40.0) * 0.1; // ±10%
      debug_cadence_rpm = DEBUG_MAX_CADENCE * (1.0 + variation);
    }
    
    if (debug_simulate_torque) {
      // Kleine Variation um maximale Werte
      float variation = cos((cycle_position - 0.25) * 30.0) * 0.15; // ±15%
      debug_torque_nm = DEBUG_MAX_TORQUE * (1.0 + variation);
    }
    
  } else if (cycle_position < 0.75) {
    // Phase 2: Ramp down
    debug_cycle_state = 2;
    float ramp_progress = 1.0 - ((cycle_position - 0.50) / 0.25); // 1.0 bis 0.0
    
    if (debug_simulate_pas) {
      debug_cadence_rpm = ramp_progress * DEBUG_MAX_CADENCE;
    }
    
    if (debug_simulate_torque) {
      debug_torque_nm = ramp_progress * DEBUG_MAX_TORQUE;
    }
    
  } else {
    // Phase 3: Hold low
    debug_cycle_state = 3;
    
    if (debug_simulate_pas) {
      // Sehr niedrige Werte mit kleiner Variation
      float variation = sin((cycle_position - 0.75) * 20.0) * 0.5;
      debug_cadence_rpm = max(0.0, 2.0 + variation); // 1.5-2.5 RPM
    }
    
    if (debug_simulate_torque) {
      // Sehr niedrige Werte mit kleiner Variation
      float variation = cos((cycle_position - 0.75) * 15.0) * 0.3;
      debug_torque_nm = max(0.0, 1.0 + variation); // 0.7-1.3 Nm
    }
  }
  
  // Begrenze Werte auf vernünftige Bereiche
  if (debug_simulate_pas) {
    debug_cadence_rpm = constrain(debug_cadence_rpm, 0.0, DEBUG_MAX_CADENCE);
  }
  
  if (debug_simulate_torque) {
    debug_torque_nm = constrain(debug_torque_nm, 0.0, DEBUG_MAX_TORQUE);
  }
  
  // Debug-Ausgabe alle 2 Sekunden
  static unsigned long last_debug_print = 0;
  if (now - last_debug_print > 2000) {
    last_debug_print = now;
    
    Serial.printf("DEBUG SMOOTH - Phase: %d, Pos: %.2f, Cadence: %.1f RPM, Torque: %.1f Nm\n", 
                  debug_cycle_state, cycle_position, debug_cadence_rpm, debug_torque_nm);
  }
}
