#include "ebike_controller.h"

// =============================================================================
// DEBUG OUTPUT
// =============================================================================

void print_debug_info() {
  // Output every 100th iteration (much slower debug rate for better performance at high RPM)
  if (loopCounter % 1000 != 0) return;
  
  // Debug mode indicator
  if (debug_mode) {
    Serial.print("DEBUG MODE | ");
    if (debug_simulate_pas) Serial.print("SIM-PAS ");
    if (debug_simulate_torque) Serial.print("SIM-TRQ ");
    Serial.print("| Phase:");
    Serial.print(debug_cycle_state);
  }

  Serial.print(" | States: ");
  Serial.print(a);
  Serial.print("-");
  Serial.print(b);
  Serial.print(" | Dir:");
  Serial.print(pedal_direction == 1 ? "FWD" : (pedal_direction == -1 ? "REV" : "STOP"));
  Serial.print(" | Pos:");
  Serial.print(pos);
  
  // Cadence information
  Serial.print(" | Cadence:");
  Serial.print(current_cadence_rpm, 1);
  Serial.print("rpm");
  if (debug_mode && debug_simulate_pas) {
    Serial.print("(sim)");
  }
  
  // Speed information
  Serial.print(" | Speed:");
  Serial.print(current_speed_kmh, 1);
  Serial.print("km/h");
  if (!vesc_data_valid) {
    Serial.print("(!)");  // Warning for invalid VESC data
  }
  
  // Torque sensor information
  Serial.print(" | Torque:");
  Serial.print(filtered_torque, 1);
  Serial.print("Nm(raw:");
  Serial.print(raw_torque_value);
  Serial.print(")");
  if (debug_mode && debug_simulate_torque) {
    Serial.print("(sim)");
  }
  
  // Power calculation
  Serial.print(" | Human:");
  Serial.print(human_power_watts, 0);
  Serial.print("W Assist:");
  Serial.print(assist_power_watts, 0);
  Serial.print("W");
  
  // Motor status and current
  Serial.print(" | Motor:");
  Serial.print(motor_enabled ? "ON" : "OFF");
  Serial.print("(");
  Serial.print(actual_current_amps, 1);
  Serial.print("A)");
  
  // Speed-dependent assist factor
  Serial.print(" | Mode:");
  Serial.print(current_mode);
  Serial.print("(x");
  Serial.print(dynamic_assist_factor, 2);  // Show current factor
  Serial.print(")");
  
  Serial.print(" | Light:");
  Serial.print(lightOn ? "ON" : "OFF");
  
  // Battery information
  Serial.print(" | Batt:");
  Serial.print(battery_voltage, 1);
  Serial.print("V(");
  Serial.print(battery_percentage, 0);
  Serial.print("%)");
  if (battery_critical) {
    Serial.print("[CRITICAL!]");
  } else if (battery_low) {
    Serial.print("[LOW!]");
  }
  
  // Original timing for compatibility
  Serial.print(" | Delay:");
  Serial.print(vescDelayBetweenList);
  
  Serial.println();
}
