#include <unity.h>
#include "test_mocks.h"

// =============================================================================
// GLOBAL MOCK VARIABLES (shared across all test modules)
// =============================================================================

// Torque sensor variables
int raw_torque_value = 2880;
float crank_torque_nm = 0.0;
float filtered_torque = 20.0;
bool debug_mode = false;
bool debug_simulate_torque = false;

// Assist calculation variables
float SPEED_POINTS_KMH[NUM_SPEED_POINTS] = {0, 5, 10, 15, 20, 30};
float ASSIST_PROFILES[3][NUM_SPEED_POINTS] = {
    {2.0, 1.8, 1.5, 1.2, 1.0, 0.8}, // Sport mode
    {1.5, 1.3, 1.1, 0.9, 0.7, 0.5}, // Eco mode  
    {1.0, 1.0, 1.0, 1.0, 1.0, 1.0}  // Linear mode
};
float current_speed_kmh = 0.0;
float current_motor_rpm = 0.0;
int current_mode = 0;
bool vesc_data_valid = true;
float dynamic_assist_factor = 1.0;
float current_cadence_rps = 1.5;
float human_power_watts = 0.0;
float assist_power_watts = 0.0;
float target_current_amps = 0.0;

// Motor control variables
bool motor_enabled = false;
float current_cadence_rpm = 0.0;
int pedal_direction = 1;
unsigned long last_pedal_activity = 0;

// Battery monitoring variables
float battery_voltage = 48.0;
float battery_percentage = 100.0;
bool battery_low = false;
bool battery_critical = false;
bool battery_led_state = false;
unsigned long last_battery_led_toggle = 0;

// VESC communication variables
float actual_current_amps = 0.0;

// Mock millis function
static unsigned long mock_millis_value = 1000;
unsigned long millis() { return mock_millis_value; }

// Mock Arduino functions
int analogRead(int pin) { (void)pin; return raw_torque_value; }
void digitalWrite(int pin, int state) { (void)pin; (void)state; }

// =============================================================================
// MOCK FUNCTION IMPLEMENTATIONS
// =============================================================================

void update_torque() {
    if (debug_mode && debug_simulate_torque) return;
    
    raw_torque_value = analogRead(36);
    int deviation_from_center = raw_torque_value - TORQUE_STANDSTILL;
    int absolute_deviation = abs(deviation_from_center);
    
    if (absolute_deviation < TORQUE_THRESHOLD) {
        crank_torque_nm = 0.0;
    } else {
        int max_deviation = max(TORQUE_STANDSTILL - TORQUE_MAX_BACKWARD, 
                               TORQUE_MAX_FORWARD - TORQUE_STANDSTILL);
        crank_torque_nm = (float)absolute_deviation / max_deviation * TORQUE_MAX_NM;
    }
    
    if (crank_torque_nm > TORQUE_MAX_NM) crank_torque_nm = TORQUE_MAX_NM;
    if (crank_torque_nm < 0.0) crank_torque_nm = 0.0;
    filtered_torque = crank_torque_nm;
}

void calculate_speed_dependent_assist() {
    if (!vesc_data_valid) {
        dynamic_assist_factor = ASSIST_PROFILES[current_mode][0];
        return;
    }
    
    int lower_index = 0, upper_index = NUM_SPEED_POINTS - 1;
    
    for (int i = 0; i < NUM_SPEED_POINTS - 1; i++) {
        if (current_speed_kmh >= SPEED_POINTS_KMH[i] && 
            current_speed_kmh <= SPEED_POINTS_KMH[i + 1]) {
            lower_index = i;
            upper_index = i + 1;
            break;
        }
    }
    
    if (current_speed_kmh <= SPEED_POINTS_KMH[0]) {
        dynamic_assist_factor = ASSIST_PROFILES[current_mode][0];
        return;
    }
    
    if (current_speed_kmh >= SPEED_POINTS_KMH[NUM_SPEED_POINTS - 1]) {
        dynamic_assist_factor = ASSIST_PROFILES[current_mode][NUM_SPEED_POINTS - 1];
        return;
    }
    
    float speed_low = SPEED_POINTS_KMH[lower_index];
    float speed_high = SPEED_POINTS_KMH[upper_index];
    float assist_low = ASSIST_PROFILES[current_mode][lower_index];
    float assist_high = ASSIST_PROFILES[current_mode][upper_index];
    
    float interpolation_factor = (current_speed_kmh - speed_low) / (speed_high - speed_low);
    dynamic_assist_factor = assist_low + interpolation_factor * (assist_high - assist_low);
    dynamic_assist_factor = constrain(dynamic_assist_factor, 0.0f, 4.0f);
}

void calculate_assist_power() {
    human_power_watts = filtered_torque * current_cadence_rps * 2.0 * PI;
    if (human_power_watts > 500.0) human_power_watts = 500.0;
    
    calculate_speed_dependent_assist();
    assist_power_watts = dynamic_assist_factor * human_power_watts;
    
    if (assist_power_watts > MAX_MOTOR_POWER) assist_power_watts = MAX_MOTOR_POWER;
    
    if (assist_power_watts > 0 && VOLTAGE_BATTERY > 0) {
        target_current_amps = assist_power_watts / VOLTAGE_BATTERY;
    } else {
        target_current_amps = 0.0;
    }
    
    target_current_amps = constrain(target_current_amps, 0.0f, (float)MAX_MOTOR_CURRENT);
    if (target_current_amps > 0 && target_current_amps < MIN_MOTOR_CURRENT) {
        target_current_amps = MIN_MOTOR_CURRENT;
    }
}

void update_motor_status() {
    unsigned long now = millis();
    bool pas_active = (now - last_pedal_activity) < PEDAL_TIMEOUT_MS;
    bool torque_present = abs(filtered_torque) > 0.2;
    bool cadence_valid = current_cadence_rpm > 2.0;
    bool mode_allows_assist = (current_mode >= 0 && current_mode < 3);
    bool forward_pedaling = pedal_direction > 0;
    
    motor_enabled = pas_active && torque_present && cadence_valid && 
                   mode_allows_assist && forward_pedaling && vesc_data_valid;
    
    if (current_cadence_rpm > 250.0) motor_enabled = false;
    if (abs(raw_torque_value - TORQUE_STANDSTILL) < TORQUE_THRESHOLD) motor_enabled = false;
    if (current_speed_kmh > 45.0) {
        motor_enabled = false;
        target_current_amps = 0.0;
    }
}

void update_battery_status() {
    battery_low = battery_percentage < BATTERY_LOW_THRESHOLD;
    battery_critical = battery_percentage < BATTERY_CRITICAL_THRESHOLD;
    
    if (battery_voltage < BATTERY_CRITICAL_VOLTAGE) {
        battery_critical = true;
    }
}

void update_battery_led() {
    unsigned long now = millis();
    
    if (!battery_low) {
        digitalWrite(BATTERY_LED_PIN, LOW);
        return;
    }
    
    unsigned long blink_interval = battery_critical ? 
        BATTERY_LED_FAST_BLINK_INTERVAL : BATTERY_LED_BLINK_INTERVAL;
    
    if (now - last_battery_led_toggle >= blink_interval) {
        battery_led_state = !battery_led_state;
        digitalWrite(BATTERY_LED_PIN, battery_led_state ? HIGH : LOW);
        last_battery_led_toggle = now;
    }
}

// =============================================================================
// TEST SETUP AND TEARDOWN
// =============================================================================

void setUp(void) {
    // Reset all variables to known state
    raw_torque_value = TORQUE_STANDSTILL;
    crank_torque_nm = 0.0;
    filtered_torque = 20.0;
    debug_mode = false;
    debug_simulate_torque = false;
    
    current_speed_kmh = 0.0;
    current_mode = 0;
    vesc_data_valid = true;
    dynamic_assist_factor = 1.0;
    current_cadence_rps = 1.5;
    human_power_watts = 0.0;
    assist_power_watts = 0.0;
    target_current_amps = 0.0;
    
    motor_enabled = false;
    current_cadence_rpm = 70.0;
    pedal_direction = 1;
    mock_millis_value = 2000;
    last_pedal_activity = mock_millis_value - 100;
    
    battery_voltage = 48.0;
    battery_percentage = 100.0;
    battery_low = false;
    battery_critical = false;
    battery_led_state = false;
    last_battery_led_toggle = 0;
    
    actual_current_amps = 0.0;
}

void tearDown(void) {
    // Clean up after each test
}

// =============================================================================
// TORQUE SENSOR TESTS
// =============================================================================

void test_torque_sensor_neutral_position(void) {
    raw_torque_value = TORQUE_STANDSTILL;
    update_torque();
    TEST_ASSERT_EQUAL_FLOAT(0.0, crank_torque_nm);
    TEST_ASSERT_EQUAL_FLOAT(0.0, filtered_torque);
}

void test_torque_sensor_below_threshold(void) {
    raw_torque_value = TORQUE_STANDSTILL + (TORQUE_THRESHOLD - 1);
    update_torque();
    TEST_ASSERT_EQUAL_FLOAT(0.0, crank_torque_nm);
    TEST_ASSERT_EQUAL_FLOAT(0.0, filtered_torque);
}

void test_torque_sensor_above_threshold(void) {
    raw_torque_value = TORQUE_STANDSTILL + TORQUE_THRESHOLD + 100;
    update_torque();
    TEST_ASSERT_TRUE(crank_torque_nm > 0.0);
    TEST_ASSERT_EQUAL_FLOAT(crank_torque_nm, filtered_torque);
}

void test_torque_sensor_maximum_forward(void) {
    raw_torque_value = TORQUE_MAX_FORWARD;
    update_torque();
    
    float expected_torque = (float)(TORQUE_MAX_FORWARD - TORQUE_STANDSTILL) / 
                           (float)max(TORQUE_STANDSTILL - TORQUE_MAX_BACKWARD, 
                                     TORQUE_MAX_FORWARD - TORQUE_STANDSTILL) * TORQUE_MAX_NM;
    
    TEST_ASSERT_FLOAT_WITHIN(1.0, expected_torque, crank_torque_nm);
    TEST_ASSERT_TRUE(crank_torque_nm > 100.0);
    TEST_ASSERT_TRUE(crank_torque_nm <= TORQUE_MAX_NM);
}

void test_torque_sensor_symmetry(void) {
    raw_torque_value = TORQUE_STANDSTILL + 500;
    update_torque();
    float torque_forward = crank_torque_nm;
    
    raw_torque_value = TORQUE_STANDSTILL - 500;
    update_torque();
    float torque_backward = crank_torque_nm;
    
    TEST_ASSERT_FLOAT_WITHIN(0.1, torque_forward, torque_backward);
}

// =============================================================================
// ASSIST CALCULATION TESTS
// =============================================================================

void test_assist_calculation_exact_speed_points(void) {
    current_speed_kmh = 0.0;
    calculate_speed_dependent_assist();
    TEST_ASSERT_EQUAL_FLOAT(2.0, dynamic_assist_factor);
    
    current_speed_kmh = 15.0;
    calculate_speed_dependent_assist();
    TEST_ASSERT_EQUAL_FLOAT(1.2, dynamic_assist_factor);
    
    current_speed_kmh = 30.0;
    calculate_speed_dependent_assist();
    TEST_ASSERT_EQUAL_FLOAT(0.8, dynamic_assist_factor);
}

void test_assist_calculation_interpolation(void) {
    current_speed_kmh = 7.5;
    calculate_speed_dependent_assist();
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1.65, dynamic_assist_factor);
    
    current_speed_kmh = 2.5;
    calculate_speed_dependent_assist();
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1.9, dynamic_assist_factor);
}

void test_assist_calculation_edge_cases(void) {
    current_speed_kmh = -5.0;
    calculate_speed_dependent_assist();
    TEST_ASSERT_EQUAL_FLOAT(2.0, dynamic_assist_factor);
    
    current_speed_kmh = 50.0;
    calculate_speed_dependent_assist();
    TEST_ASSERT_EQUAL_FLOAT(0.8, dynamic_assist_factor);
    
    vesc_data_valid = false;
    current_speed_kmh = 15.0;
    calculate_speed_dependent_assist();
    TEST_ASSERT_EQUAL_FLOAT(2.0, dynamic_assist_factor);
}

void test_power_calculation(void) {
    filtered_torque = 20.0;
    current_cadence_rps = 1.5;
    current_speed_kmh = 0.0;
    
    calculate_assist_power();
    
    TEST_ASSERT_FLOAT_WITHIN(1.0, 188.5, human_power_watts);
    // Assist power should be limited to 350W (MAX_MOTOR_POWER)
    TEST_ASSERT_FLOAT_WITHIN(1.0, 350.0, assist_power_watts);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 7.29, target_current_amps); // 350W / 48V
}

void test_power_limits(void) {
    filtered_torque = 100.0;
    current_cadence_rps = 2.0;
    current_speed_kmh = 0.0;
    
    calculate_assist_power();
    
    TEST_ASSERT_TRUE(human_power_watts <= 500.0);
    TEST_ASSERT_TRUE(assist_power_watts <= 350.0);
    TEST_ASSERT_TRUE(target_current_amps <= 8.0);
}

// =============================================================================
// MOTOR CONTROL TESTS
// =============================================================================

void test_motor_activation_normal_conditions(void) {
    mock_millis_value = 2000;
    last_pedal_activity = mock_millis_value - 100;
    filtered_torque = 15.0;
    current_cadence_rpm = 60.0;
    current_mode = 0;
    pedal_direction = 1;
    current_speed_kmh = 15.0;
    raw_torque_value = TORQUE_STANDSTILL + TORQUE_THRESHOLD + 100;
    
    update_motor_status();
    
    TEST_ASSERT_TRUE(motor_enabled);
}

void test_motor_deactivation_pas_timeout(void) {
    mock_millis_value = 5000;
    last_pedal_activity = mock_millis_value - (PEDAL_TIMEOUT_MS + 100);
    filtered_torque = 15.0;
    current_cadence_rpm = 60.0;
    current_mode = 0;
    pedal_direction = 1;
    raw_torque_value = TORQUE_STANDSTILL + TORQUE_THRESHOLD + 100;
    
    update_motor_status();
    
    TEST_ASSERT_FALSE(motor_enabled);
}

void test_motor_deactivation_reverse_pedaling(void) {
    mock_millis_value = 2000;
    last_pedal_activity = mock_millis_value - 100;
    filtered_torque = 15.0;
    current_cadence_rpm = 60.0;
    current_mode = 0;
    pedal_direction = -1; // Reverse pedaling
    raw_torque_value = TORQUE_STANDSTILL + TORQUE_THRESHOLD + 100;
    
    update_motor_status();
    
    TEST_ASSERT_FALSE(motor_enabled);
}

void test_emergency_speed_cutoff(void) {
    mock_millis_value = 2000;
    last_pedal_activity = mock_millis_value - 100;
    filtered_torque = 15.0;
    current_cadence_rpm = 60.0;
    current_mode = 0;
    pedal_direction = 1;
    current_speed_kmh = 50.0; // Excessive speed
    raw_torque_value = TORQUE_STANDSTILL + TORQUE_THRESHOLD + 100;
    
    update_motor_status();
    
    TEST_ASSERT_FALSE(motor_enabled);
    TEST_ASSERT_EQUAL_FLOAT(0.0, target_current_amps);
}

// =============================================================================
// BATTERY MONITORING TESTS
// =============================================================================

void test_normal_battery_status(void) {
    battery_voltage = 52.0;
    battery_percentage = 80.0;
    
    update_battery_status();
    
    TEST_ASSERT_FALSE(battery_low);
    TEST_ASSERT_FALSE(battery_critical);
}

void test_low_battery_detection(void) {
    battery_voltage = 45.0;
    battery_percentage = 15.0;
    
    update_battery_status();
    
    TEST_ASSERT_TRUE(battery_low);
    TEST_ASSERT_FALSE(battery_critical);
}

void test_critical_battery_detection(void) {
    battery_voltage = 41.0;
    battery_percentage = 5.0;
    
    update_battery_status();
    
    TEST_ASSERT_TRUE(battery_low);
    TEST_ASSERT_TRUE(battery_critical);
}

void test_battery_led_normal(void) {
    battery_low = false;
    battery_critical = false;
    mock_millis_value = 2000;
    
    update_battery_led();
    
    // LED should be off for normal battery
    TEST_ASSERT_FALSE(battery_led_state);
}

// =============================================================================
// INTEGRATION TESTS
// =============================================================================

void test_complete_sensor_fusion_pipeline(void) {
    // Setup normal riding conditions
    current_speed_kmh = 15.0;
    filtered_torque = 25.0;
    current_cadence_rps = 70.0 / 60.0;
    current_mode = 0; // Sport mode
    
    // Step 1: Calculate assist factor
    calculate_speed_dependent_assist();
    TEST_ASSERT_EQUAL_FLOAT(1.2, dynamic_assist_factor);
    
    // Step 2: Calculate power
    calculate_assist_power();
    TEST_ASSERT_FLOAT_WITHIN(5.0, 183.3, human_power_watts);
    TEST_ASSERT_TRUE(assist_power_watts <= 350.0); // Should hit motor limit
    
    // Step 3: Check motor status
    mock_millis_value = 2000;
    last_pedal_activity = mock_millis_value - 100;
    current_cadence_rpm = 70.0;
    pedal_direction = 1;
    raw_torque_value = TORQUE_STANDSTILL + TORQUE_THRESHOLD + 100;
    
    update_motor_status();
    TEST_ASSERT_TRUE(motor_enabled);
}

void test_different_assist_modes(void) {
    current_speed_kmh = 10.0;
    
    // Test Sport mode
    current_mode = 0;
    calculate_speed_dependent_assist();
    float sport_factor = dynamic_assist_factor;
    
    // Test Eco mode
    current_mode = 1;
    calculate_speed_dependent_assist();
    float eco_factor = dynamic_assist_factor;
    
    // Test Linear mode
    current_mode = 2;
    calculate_speed_dependent_assist();
    float linear_factor = dynamic_assist_factor;
    
    TEST_ASSERT_TRUE(sport_factor > eco_factor);
    TEST_ASSERT_EQUAL_FLOAT(1.0, linear_factor);
    TEST_ASSERT_EQUAL_FLOAT(1.5, sport_factor);
    TEST_ASSERT_EQUAL_FLOAT(1.1, eco_factor);
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

int main(void) {
    UNITY_BEGIN();
    
    // Torque Sensor Tests
    RUN_TEST(test_torque_sensor_neutral_position);
    RUN_TEST(test_torque_sensor_below_threshold);
    RUN_TEST(test_torque_sensor_above_threshold);
    RUN_TEST(test_torque_sensor_maximum_forward);
    RUN_TEST(test_torque_sensor_symmetry);
    
    // Assist Calculation Tests
    RUN_TEST(test_assist_calculation_exact_speed_points);
    RUN_TEST(test_assist_calculation_interpolation);
    RUN_TEST(test_assist_calculation_edge_cases);
    RUN_TEST(test_power_calculation);
    RUN_TEST(test_power_limits);
    
    // Motor Control Tests
    RUN_TEST(test_motor_activation_normal_conditions);
    RUN_TEST(test_motor_deactivation_pas_timeout);
    RUN_TEST(test_motor_deactivation_reverse_pedaling);
    RUN_TEST(test_emergency_speed_cutoff);
    
    // Battery Monitoring Tests
    RUN_TEST(test_normal_battery_status);
    RUN_TEST(test_low_battery_detection);
    RUN_TEST(test_critical_battery_detection);
    RUN_TEST(test_battery_led_normal);
    
    // Integration Tests
    RUN_TEST(test_complete_sensor_fusion_pipeline);
    RUN_TEST(test_different_assist_modes);
    
    return UNITY_END();
}
