#ifndef TEST_MOCKS_H
#define TEST_MOCKS_H

// Mock Arduino and ESP32 definitions for native testing
#ifndef ARDUINO
#define ARDUINO 100
#endif

#ifndef ESP32
#define ESP32
#endif

// Mock Arduino functions and types
#define LOW 0
#define HIGH 1
#define PI 3.14159265359

// Mock ESP32/Arduino functions for testing
extern "C"
{
    unsigned long millis();
    unsigned long micros();
    int analogRead(int pin);
    void digitalWrite(int pin, int value);
    void pinMode(int pin, int mode);
    int digitalRead(int pin);
}

// Mock constrain function
template <typename T>
T constrain(T value, T min_val, T max_val)
{
    if (value < min_val)
        return min_val;
    if (value > max_val)
        return max_val;
    return value;
}

// Mock max function
template <typename T>
T max(T a, T b)
{
    return (a > b) ? a : b;
}

// Mock min function
template <typename T>
T min(T a, T b)
{
    return (a < b) ? a : b;
}

// Mock abs function
template <typename T>
T abs(T value)
{
    return (value < 0) ? -value : value;
}

// Tailwind controller constants (copied from tailwind_controller.h)
#define NUM_SPEED_POINTS 6
#define TORQUE_STANDSTILL_DEFAULT 2880 // Default standstill for tests
#define TORQUE_MAX_FORWARD 4095
#define TORQUE_MAX_BACKWARD 0
#define TORQUE_MAX_NM 100.0
#define TORQUE_THRESHOLD 30
#define VOLTAGE_BATTERY 48.0
#define MAX_MOTOR_POWER 350.0
#define MAX_MOTOR_CURRENT 15.0
#define MIN_MOTOR_CURRENT 0.1
#define BATTERY_LOW_THRESHOLD 20.0
#define BATTERY_CRITICAL_THRESHOLD 10.0
#define BATTERY_FULL_VOLTAGE 54.6
#define BATTERY_CRITICAL_VOLTAGE 40.8
#define BATTERY_LED_PIN 4
#define BATTERY_LED_BLINK_INTERVAL 500
#define BATTERY_LED_FAST_BLINK_INTERVAL 200
#define PEDAL_TIMEOUT_MS 1000
#define CADENCE_WINDOW_MS 1000
#define MODE_SWITCH_STEPS 3

// Mock function declarations for Tailwind controller
void update_torque();
void calculate_speed_dependent_assist();
void calculate_assist_power();
void update_motor_status();
void update_vesc_data();
void update_battery_status();
void update_battery_led();
void update_debug_simulation();

// Mock variables for torque calibration (for testing)
extern int torque_standstill_calibrated;
extern bool torque_calibration_complete;
extern int torque_calibration_count;

#endif // TEST_MOCKS_H
