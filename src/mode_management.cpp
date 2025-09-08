#include "ebike_controller.h"

// WiFi Logging Integration
#include "wifi_telemetry.h"

// =============================================================================
// MODE SWITCHING
// =============================================================================

void update_mode_selection() {
  // Mode switch logic: Single mode change per reverse session
  static bool mode_switched_this_session = false;  // Flag for current reverse cycle
  
  if (pos <= -MODE_SWITCH_STEPS) {
    // Mode change only if not already switched in this session
    if (!mode_switched_this_session) {
      // Increase mode by 1 (relative)
      int new_mode = current_mode + 1;
      
      // Wrap around: Only cycle through active profiles
      if (new_mode >= NUM_ACTIVE_PROFILES) {
        new_mode = 0;
      }
      
      current_mode = new_mode;
      mode_switched_this_session = true;  // Mark as switched
      
      Serial.print("MODE CHANGED: ");
      Serial.print(current_mode);
      Serial.print(" (Reverse steps: ");
      Serial.print(-pos);
      Serial.println(")");
      
      addLogMessage("Mode switched to: " + String(current_mode) + " (Reverse steps: " + String(-pos) + ")");
    }
  } else {
    // Position reset (=0) - reset session flag for next cycle
    mode_switched_this_session = false;
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
    // Update shared sensor data (thread-safe)
    if (xSemaphoreTake(dataUpdateSemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
      sharedSensorData.current_mode = new_mode;
      current_mode = new_mode;  // Also update global variable for compatibility
      
      // Set light according to new mode
      lightOn = AVAILABLE_PROFILES[new_mode].hasLight;
      digitalWrite(LIGHT_PIN, lightOn ? HIGH : LOW);
      
      xSemaphoreGive(dataUpdateSemaphore);
      
      Serial.printf("External mode change to: %d (%s)\n", new_mode, AVAILABLE_PROFILES[new_mode].name);
    }
  }
}
