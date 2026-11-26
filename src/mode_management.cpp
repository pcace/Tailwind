#include "tailwind_controller.h"

#include <atomic>

// WiFi Logging Integration
#include "wifi_telemetry.h"

namespace {
// The sensor task owns the active mode and applies requests from BLE.
std::atomic<int> requested_mode{-1};
}

// =============================================================================
// MODE SWITCHING
// =============================================================================

void update_mode_selection() {
  if (reverse_steps >= MODE_SWITCH_STEPS) {
    reverse_steps -= MODE_SWITCH_STEPS;
    ++current_mode;
    if (current_mode >= NUM_ACTIVE_PROFILES) {
      current_mode = MODE_URBAN;
    }

    Serial.printf("MODE CHANGED: %d (Reverse steps: %d)\n", current_mode, MODE_SWITCH_STEPS);
    addLogMessage("Mode switched to: " + String(current_mode) + " (Reverse steps: " + String(MODE_SWITCH_STEPS) + ")");
    indicate_mode_change(current_mode);
  }

  const int external_mode = requested_mode.exchange(-1, std::memory_order_relaxed);
  if (external_mode >= 0) {
    reverse_steps = 0;
    current_mode = external_mode;
    Serial.printf("External mode change to: %d (%s)\n", current_mode, AVAILABLE_PROFILES[current_mode].name);
    indicate_mode_change(current_mode);
  }
  
  // Set light according to mode
  lightOn = LIGHT_MODES[current_mode];
  digitalWrite(LIGHT_PIN, lightOn ? HIGH : LOW);
}

// =============================================================================
// EXTERNAL MODE CHANGE INTERFACE (for WiFi/BLE)
// =============================================================================

// Mode change function for external interfaces (WiFi, BLE)
void changeAssistMode(int new_mode) {
  if (new_mode >= 0 && new_mode < NUM_ACTIVE_PROFILES) {
    requested_mode.store(new_mode, std::memory_order_relaxed);
  }
}
