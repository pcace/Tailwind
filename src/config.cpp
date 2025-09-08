#include "ebike_controller.h"
#include <VescUart.h>

// =============================================================================
// TELEMETRY CONFIGURATION
// =============================================================================

// WiFi Web Interface - set to true to enable
bool enable_wifi_telemetry = false; // Set to true to enable WiFi Web Interface

// BLE (Bluetooth Low Energy) Interface - set to true to enable
bool enable_ble_telemetry = true; // Set to true to enable BLE Interface

// VESC Bridge Mode - set to true to enable direct VESC access via ESP32
bool vesc_bridge_mode = false; // Set to true to enable VESC Bridge Mode

// NOTE: When vesc_bridge_mode is enabled, the ESP32 acts as a transparent
// bridge between your computer and the VESC controller. This allows you to
// use VESC Tool directly through the ESP32's USB connection.
// WARNING: When bridge mode is active, the normal e-bike controller is disabled!

// NOTE: Both WiFi and BLE can be enabled simultaneously, but this requires
// the "huge_app.csv" partition scheme in platformio.ini to fit in flash memory.
// If memory is tight, disable one of them:
// - WiFi: Best for web browser monitoring, multiple connections, debugging
// - BLE: Best for mobile apps, lower power consumption, single connection

// =============================================================================
// GLOBAL VARIABLE DEFINITIONS AND CONFIGURATION
// =============================================================================

// Speed interpolation points [km/h]
float SPEED_POINTS_KMH[NUM_SPEED_POINTS] = {0, 5, 10, 15, 20, 30};

// Available assist profiles - comment out profiles you don't want to use
// The system will automatically use only the enabled profiles
AssistProfile AVAILABLE_PROFILES[] = {

    {"Urban",
     "City riding",
     false,
     {6, 4, 2, 1.0, 3, 6}},

    {"Linear",
     "Linear power",
     true,
     {3.0, 3.0, 3.0, 3.0, 3.0, 3.0}},

    {"Touring Eco",
     "Eco touring",
     true,
     {1.8, 1.2, 1.0, 0.8, 0.7, 0.5}},

    {"No Assist",
     "No assist",
     false,
     {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
};

// Calculate number of active profiles at compile time
const int NUM_ACTIVE_PROFILES = sizeof(AVAILABLE_PROFILES) / sizeof(AVAILABLE_PROFILES[0]);

// Legacy arrays for compatibility with existing code (dynamically sized)
float ASSIST_PROFILES[10][NUM_SPEED_POINTS]; // Max 10 profiles (should be enough)
bool LIGHT_MODES[10];

// Function to initialize legacy arrays from active profiles
void initializeAssistProfiles()
{
  // Clear all profiles first (use a reasonable maximum)
  for (int i = 0; i < 10; i++)
  {
    LIGHT_MODES[i] = false;
    for (int j = 0; j < NUM_SPEED_POINTS; j++)
    {
      ASSIST_PROFILES[i][j] = 0.0;
    }
  }

  // Copy active profiles to legacy arrays
  for (int i = 0; i < NUM_ACTIVE_PROFILES; i++)
  {
    LIGHT_MODES[i] = AVAILABLE_PROFILES[i].hasLight;
    for (int j = 0; j < NUM_SPEED_POINTS; j++)
    {
      ASSIST_PROFILES[i][j] = AVAILABLE_PROFILES[i].profile[j];
    }
  }
}

// PAS sensor state variables
int pos = 0;
int a = 0, b = 0;
int pedal_direction = 0; // 1=forward, -1=backward, 0=standstill
unsigned long last_pulse_time = 0;
unsigned long pulse_intervals[4] = {0, 0, 0, 0};
int pulse_index = 0;

// Interrupt-based PAS sensor variables
volatile bool pas_interrupt_flag = false;
volatile unsigned long last_interrupt_time = 0;
volatile int quadrature_pulses_per_rev = 32; // 8 original pulses × 4 quadrature transitions
volatile unsigned long last_revolution_time = 0;

// Sensor measurements
float current_cadence_rpm = 0.0;
float current_cadence_rps = 0.0;
int raw_torque_value = 0;
float crank_torque_nm = 0.0;
float filtered_torque = 0.0;

// Torque sensor calibration variables
int torque_standstill_calibrated = TORQUE_STANDSTILL_DEFAULT; // Start with default value
bool torque_calibration_complete = false;                     // Calibration not done yet
int torque_calibration_count = 0;                             // Sample counter

// Speed and assist
float current_speed_kmh = 0.0;
float current_motor_rpm = 0.0;
float dynamic_assist_factor = 1.0;
bool vesc_data_valid = false;

// Power calculation
float human_power_watts = 0.0;
float assist_power_watts = 0.0;
float target_current_amps = 0.0;
float actual_current_amps = 0.0;

// System status
int current_mode = 0;
bool motor_enabled = false;
bool lightOn = false;
unsigned long last_pedal_activity = 0;
unsigned long last_loop_time = 0;
unsigned long last_vesc_data_time = 0;

// Battery monitoring
float battery_voltage = 0.0;
float battery_percentage = 100.0;
bool battery_low = false;
bool battery_critical = false;
bool battery_led_state = false;
unsigned long last_battery_led_toggle = 0;

// Debug/Compatibility
int loopCounter = 0;
int vescCounter = 0;
int vescDelayBetween = 9999;
int vescDelayBetweenList = 9999;

// =============================================================================
// DEBUG MODE VARIABLES
// =============================================================================
bool debug_mode = true;             // Set to true to enable debug mode
bool debug_simulate_pas = false;    // Simulate PAS sensor when in debug mode
bool debug_simulate_torque = false; // Simulate torque sensor when in debug mode

// Debug simulation mode selection
debug_mode_type_t debug_simulation_mode = DEBUG_MODE_SYSTEMATIC_TEST; // Change this to switch modes

// Debug simulation variables
float debug_cadence_rpm = 0.0;
float debug_torque_nm = 0.0;
unsigned long debug_last_update = 0;
int debug_cycle_state = 0; // 0=ramp up, 1=hold high, 2=ramp down, 3=hold low

// Systematic test variables
int debug_test_cadence_index = 0;
int debug_test_torque_index = 0;
unsigned long debug_test_start_time = 0;
bool debug_test_completed = false;
