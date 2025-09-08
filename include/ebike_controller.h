#ifndef EBIKE_CONTROLLER_H
#define EBIKE_CONTROLLER_H

#include <Arduino.h>

// ESP32 FreeRTOS Headers - verwende die echten ESP32 FreeRTOS Typen
#ifdef ESP32
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/semphr.h"
#else
  #error "This multi-core implementation is designed for ESP32 only"
#endif

// =============================================================================
// E-BIKE CONFIGURATION
// =============================================================================

// Hardware-specific constants
#define VOLTAGE_BATTERY     48.0   // Battery voltage [V] (48V 13S2P)
#define MAX_MOTOR_POWER     350.0  // Q100C motor rated power [W]
#define MAX_MOTOR_CURRENT   15.0    // Maximum motor current for Q100C [A]
#define MIN_MOTOR_CURRENT   0.1    // Minimum motor current [A]

// Battery monitoring configuration
#define BATTERY_LOW_THRESHOLD   20.0   // Low battery threshold [%]
#define BATTERY_CRITICAL_THRESHOLD 10.0 // Critical battery threshold [%] - fast blinking
#define BATTERY_CRITICAL_VOLTAGE 40.8  // Critical voltage for 48V battery (20% = ~40.8V)
#define BATTERY_FULL_VOLTAGE     54.6  // Full voltage for 48V battery (100% = 54.6V)
#define BATTERY_LED_BLINK_INTERVAL 500 // LED blink interval in ms for low battery
#define BATTERY_LED_FAST_BLINK_INTERVAL 200 // LED fast blink interval in ms for critical battery

// Torque sensor calibration (corrected values after GND connection)
#define TORQUE_SENSOR_PIN   36     // Analog pin for torque sensor on ESP32 (ADC1_CH0, SVP)
#define TORQUE_STANDSTILL_DEFAULT 2055  // Default ADC value at neutral position (ESP32: 12-bit ADC = 0-4095, 3.3V)
#define TORQUE_MAX_FORWARD  4095   // ADC value at maximum forward torque
#define TORQUE_MAX_BACKWARD 0      // ADC value at maximum backward torque  
#define TORQUE_MAX_NM       100.0  // Maximum torque [Nm] - Based on 100kg test (87Nm real) with safety margin
#define TORQUE_THRESHOLD    20     // Minimum deviation from standstill for valid signal (~3Nm sensitivity)

// Torque sensor calibration settings
#define TORQUE_CALIBRATION_SAMPLES  20     // Number of samples for calibration
#define TORQUE_CALIBRATION_DELAY_MS 50     // Delay between calibration samples [ms]
#define TORQUE_CALIBRATION_TIMEOUT_MS 5000 // Maximum calibration time [ms]

// PAS sensor configuration
#define CADENCE_WINDOW_MS   1000   // Time window for cadence calculation [ms]
#define PEDAL_TIMEOUT_MS    1000   // Max. time without pedal activity [ms]
#define MODE_SWITCH_STEPS   32      // Number of reverse steps for mode switching

// Speed-dependent assist configuration
#define NUM_SPEED_POINTS    6      // Number of speed interpolation points

// Hardware pins (ESP32 DevKit v1 Pin Layout)
// Note: 5V sensors need logic level converter for ESP32 (3.3V)
#define PAS_PIN_A          18      // GPIO18 - Hall sensor A (interrupt capable)
#define PAS_PIN_B          19      // GPIO19 - Hall sensor B (interrupt capable)  
#define LIGHT_PIN          2       // GPIO2 - Built-in LED (status/debug)
#define HEADLIGHT_PIN      25      // GPIO25 - Headlight MOSFET control (PWM capable)
#define BATTERY_LED_PIN    4       // GPIO4 - Battery status LED pin
#define WHEEL_SPEED_PIN    5       // GPIO5 - Wheel speed sensor (future use, interrupt capable)
// VESC uses Hardware UART2: RX=GPIO16 (RX2), TX=GPIO17 (TX2)

// Motor parameters for Q100C (from Motor.md)
#define MOTOR_GEAR_RATIO    14.2   // Q100C gear ratio
#define MOTOR_POLES         16     // Number of poles (16 poles = 8 pole pairs)
#define WHEEL_DIAMETER_M    0.72   // 28" = 720mm diameter

// Motor constants for Q100C at 48V operation (from performance curve data)
// Real measured data from Q100C performance curve (July 2013):
// Max efficiency: 7.17 Nm @ 5.28 A → K_t = 1.36 Nm/A
// Max torque: 20.04 Nm @ 13.37 A → K_t = 1.50 Nm/A
// Average motor constant: K_t ≈ 1.43 Nm/A
#define MOTOR_CONSTANT_KT   1.43   // Torque constant [Nm/A] - from Q100C performance data

// =============================================================================
// FREERTOS MULTI-CORE DECLARATIONS
// =============================================================================

// Task handles
extern TaskHandle_t sensorTaskHandle;
extern TaskHandle_t vescTaskHandle;

// Semaphores for thread-safe data sharing
extern SemaphoreHandle_t dataUpdateSemaphore;
extern SemaphoreHandle_t motorCommandSemaphore;

// Shared data structures (protected by semaphores)
struct SharedSensorData {
  float cadence_rpm;
  float cadence_rps;
  float torque_nm;
  float filtered_torque;
  int current_mode;
  bool motor_enabled;
  unsigned long last_update;
};

struct SharedVescData {
  float speed_kmh;
  bool data_valid;
  float actual_current;
  float battery_voltage;
  float battery_percentage;
  
  // Extended VESC data for web interface
  float rpm;
  float duty_cycle;
  float temp_mosfet;
  float temp_motor;
  float amp_hours;
  float watt_hours;
  
  unsigned long last_update;
};

struct SharedMotorCommand {
  float target_current;
  bool command_ready;
  unsigned long timestamp;
  bool test_mode;
  unsigned long test_end_time;
};

extern SharedSensorData sharedSensorData;
extern SharedVescData sharedVescData;
extern SharedMotorCommand sharedMotorCommand;

// =============================================================================
// TELEMETRY CONFIGURATION (optional)
// =============================================================================

// WiFi Web Interface - enable to get browser-based monitoring
extern bool enable_wifi_telemetry;      // Enable WiFi Web Interface with logging

// BLE Interface - enable to get Bluetooth Low Energy monitoring
extern bool enable_ble_telemetry;       // Enable BLE Interface with telemetry

// VESC Bridge Mode - enable to use ESP32 as direct VESC access bridge
extern bool vesc_bridge_mode;           // Enable VESC Bridge Mode (disables e-bike controller)

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

// =============================================================================
// SPEED-DEPENDENT ASSIST PROFILES
// =============================================================================

// Assist profile structure
struct AssistProfile {
  const char* name;
  const char* description;
  bool hasLight;
  float profile[NUM_SPEED_POINTS];
};

// Available assist profiles (defined in config.cpp)
extern AssistProfile AVAILABLE_PROFILES[];

// Speed points for interpolation [km/h]
extern float SPEED_POINTS_KMH[NUM_SPEED_POINTS];

// Number of currently active assist profiles (calculated at compile time)
extern const int NUM_ACTIVE_PROFILES;

// Assist factors for each mode [%] (dynamically sized based on active profiles)
// [0] = Profile 1, [1] = Profile 2, etc. (based on NUM_ACTIVE_PROFILES)
extern float ASSIST_PROFILES[][NUM_SPEED_POINTS];

// Light modes per assist mode (dynamically sized)
extern bool LIGHT_MODES[];

// PAS sensor state variables
extern int pos;                    // Pedal position (for mode switching)
extern int a, b;                   // Hall sensor states
extern int pedal_direction;        // Current pedal direction: 1=forward, -1=backward, 0=standstill
extern unsigned long last_pulse_time;
extern unsigned long pulse_intervals[4];
extern int pulse_index;

// Interrupt-based PAS sensor variables
extern volatile bool pas_interrupt_flag;  // Flag for new PAS data
extern volatile unsigned long last_interrupt_time;  // Time of last interrupt
extern volatile int quadrature_pulses_per_rev;  // Actual pulses per revolution (32 with quadrature)
extern volatile unsigned long last_revolution_time;  // Time of last full revolution

// Sensor measurements
extern float current_cadence_rpm;     // Current cadence [RPM]
extern float current_cadence_rps;     // Current cadence [RPS]
extern int raw_torque_value;          // Raw ADC value (0-1023)
extern float crank_torque_nm;         // Torque [Nm]
extern float filtered_torque;         // Filtered torque

// Torque sensor calibration variables
extern int torque_standstill_calibrated;   // Calibrated neutral position value
extern bool torque_calibration_complete;   // Calibration status flag
extern int torque_calibration_count;       // Current calibration sample count

// Speed and assist
extern float current_speed_kmh;       // Current speed [km/h]
extern float current_motor_rpm;       // Current motor RPM (for motor current calculation)
extern float dynamic_assist_factor;   // Current assist factor
extern bool vesc_data_valid;          // VESC data valid?

// Power calculation
extern float human_power_watts;       // Human power [W]
extern float assist_power_watts;      // Target motor power [W]
extern float target_current_amps;     // Target motor current [A]
extern float actual_current_amps;     // Actual motor current [A]

// System status
extern int current_mode;              // Current assist mode (0 to NUM_ACTIVE_PROFILES-1)
extern bool motor_enabled;            // Motor on/off
extern bool lightOn;                  // Light status
extern unsigned long last_pedal_activity;
extern unsigned long last_loop_time;
extern unsigned long last_vesc_data_time;

// Battery monitoring
extern float battery_voltage;         // Current battery voltage [V]
extern float battery_percentage;      // Current battery level [%]
extern bool battery_low;              // Battery low warning flag (≤20%)
extern bool battery_critical;         // Battery critical warning flag (≤10%)
extern bool battery_led_state;        // Current LED state for blinking
extern unsigned long last_battery_led_toggle; // Last LED toggle time

// Debug/Compatibility
extern int loopCounter;
extern int vescCounter;
extern int vescDelayBetween;
extern int vescDelayBetweenList;

// =============================================================================
// DEBUG MODE CONFIGURATION
// =============================================================================
extern bool debug_mode;                    // Main debug flag
extern bool debug_simulate_pas;            // Simulate PAS sensor
extern bool debug_simulate_torque;         // Simulate torque sensor

// Debug simulation modes
typedef enum {
  DEBUG_MODE_SMOOTH_CYCLE,     // Original smooth cycling mode
  DEBUG_MODE_SYSTEMATIC_TEST   // Systematic cadence/torque testing
} debug_mode_type_t;

extern debug_mode_type_t debug_simulation_mode;  // Which debug mode to use

// Debug simulation variables
extern float debug_cadence_rpm;            // Simulated cadence
extern float debug_torque_nm;              // Simulated torque
extern unsigned long debug_last_update;    // Last debug update time
extern int debug_cycle_state;              // Current state in debug cycle

// Systematic test variables
extern int debug_test_cadence_index;       // Current cadence being tested
extern int debug_test_torque_index;        // Current torque being tested
extern unsigned long debug_test_start_time; // When current test started
extern bool debug_test_completed;          // Flag if all tests completed

// Debug cycle configuration
#define DEBUG_CYCLE_DURATION_MS    20000   // Total cycle duration (20 seconds) - smooth mode
#define DEBUG_UPDATE_INTERVAL_MS   100     // Update every 100ms
#define DEBUG_MAX_CADENCE         80.0     // Maximum cadence in simulation - smooth mode
#define DEBUG_MAX_TORQUE          40.0     // Maximum torque in simulation - smooth mode

// Systematic test configuration
#define DEBUG_TEST_DURATION_MS     5000    // How long to hold each test point (5 seconds)
#define DEBUG_TEST_CADENCES        {10, 20, 30, 40, 50, 60, 70, 80}  // Cadence values to test [RPM]
#define DEBUG_TEST_TORQUES         {5, 10, 15, 20, 25, 30, 35, 40}   // Torque values to test [Nm]
#define DEBUG_TEST_CADENCE_COUNT   8       // Number of cadence values
#define DEBUG_TEST_TORQUE_COUNT    8       // Number of torque values

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================

// FreeRTOS Task functions
void sensorTask(void *pvParameters);
void vescTask(void *pvParameters);

// Initialization
void ebike_setup();
void startup_battery_indicator();     // Show battery level at startup
void initializeAssistProfiles(); // Initialize assist profiles from active configuration

// Sensor functions
void read_pas_sensors();
void pas_interrupt_handler();      // Interrupt handler for PAS sensors (ESP32 doesn't need IRAM_ATTR by default)
void update_cadence();
void update_torque();

// Torque sensor calibration functions
void calibrate_torque_sensor();    // Perform initial torque sensor calibration
bool is_torque_calibration_complete(); // Check if calibration is finished

// VESC communication
void update_vesc_data();

// Battery monitoring
void update_battery_status();
void update_battery_led();

// Assist calculation
void calculate_speed_dependent_assist();
void calculate_assist_power();

// Motor control
void update_motor_status();
void send_motor_command();

// Mode management
void update_mode_selection();
void changeAssistMode(int new_mode);      // External mode change interface for WiFi/BLE

// Debug
void print_debug_info();
void update_debug_simulation();            // Update debug simulation values
void update_systematic_test_simulation(unsigned long now);  // Systematic test mode
void update_smooth_cycle_simulation(unsigned long now);     // Smooth cycle mode

// =============================================================================
// VESC BRIDGE MODE DECLARATIONS
// =============================================================================

// VESC Bridge Mode functions (defined in vesc_bridge.cpp)
void initVescBridge();
void runVescBridge();
void shutdownVescBridge();

#endif // EBIKE_CONTROLLER_H
