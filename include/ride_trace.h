#pragma once

#include <cstdint>

struct RideSensorSample {
  uint32_t sensor_seq;
  uint32_t sensor_ms;
  int mode;
  int pas_position;
  int pedal_direction;
  uint32_t pas_age_ms;
  int torque_adc;
  int torque_zero_adc;
  float torque_nm;
  float filtered_torque_nm;
  float cadence_rpm;
  float rider_w;
  float assist_factor;
  float assist_w;
  float target_a;
  float commanded_a;
  bool motor_enabled;
  float speed_kmh;
  float motor_rpm;
  bool speed_valid;
  bool command_published;
};

struct RideCommandSample {
  uint32_t sensor_seq;
  uint32_t sensor_ms;
  float current_a;
  uint32_t write_us;
};

struct RideVescSample {
  uint32_t query_us;
  bool reply;
  bool speed_valid;
  float speed_kmh;
  float erpm;
  float motor_a;
  float battery_a;
  float battery_v;
  float duty;
  float fet_c;
  float watt_hours;
  float watt_hours_charged;
  int fault;
};

#ifdef TAILWIND_USB_LOGGING
void setupRideTrace();
void traceRideSensor(const RideSensorSample& sample);
void traceRideCommand(const RideCommandSample& sample);
void traceRideVesc(const RideVescSample& sample);
#endif
