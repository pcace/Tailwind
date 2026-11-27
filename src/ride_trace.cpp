#include "ride_trace.h"

#ifdef TAILWIND_USB_LOGGING
#include "boot_diagnostics.h"
#include <Arduino.h>
#include <cinttypes>
#include <cstdio>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace {

enum class RecordKind { Sensor, Command, Vesc };

struct TraceRecord {
  RecordKind kind;
  uint32_t sequence;
  int64_t uptime_us;
  union {
    RideSensorSample sensor;
    RideCommandSample command;
    RideVescSample vesc;
  };
};

constexpr size_t QUEUE_DEPTH = 64;
constexpr size_t LINE_CAPACITY = 640;
constexpr int64_t BOOT_INTERVAL_US = 5000000;
StaticQueue_t queueControl;
alignas(TraceRecord) uint8_t queueStorage[QUEUE_DEPTH * sizeof(TraceRecord)];
QueueHandle_t traceQueue = nullptr;

constexpr char BOOT_HEADER[] = "\n# TW1,B,boot_id,reset_reason,uptime_us\n";
constexpr char SENSOR_HEADER[] =
  "\n# TW1,S,boot_id,seq,uptime_us,sensor_ms,mode,pas_position,pedal_direction,"
  "pas_age_ms,torque_adc,torque_zero_adc,torque_nm,filtered_torque_nm,cadence_rpm,"
  "rider_w,assist_factor,assist_w,target_a,commanded_a,motor_enabled,speed_kmh,"
  "motor_rpm,speed_valid,command_published\n";
constexpr char COMMAND_HEADER[] =
  "\n# TW1,C,boot_id,seq,uptime_us,sensor_seq,sensor_ms,current_a,write_us\n";
constexpr char VESC_HEADER[] =
  "\n# TW1,V,boot_id,seq,uptime_us,query_us,reply,speed_valid,speed_kmh,erpm,motor_a,"
  "battery_a,battery_v,duty,fet_c,watt_hours,watt_hours_charged,fault\n";

void writeTraceLine(const char* line, size_t length)
{
  // Arduino can report free FIFO space while its software TX buffer is full.
  // Keep enough headroom to exceed that fallback and leave room for other USB messages.
  constexpr int TX_HEADROOM = 128;
  while (Serial.availableForWrite() < static_cast<int>(length) + TX_HEADROOM) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  Serial.write(reinterpret_cast<const uint8_t*>(line), length);
}

int formatRecord(char* line, size_t capacity, uint32_t bootId, const TraceRecord& record)
{
  switch (record.kind) {
    case RecordKind::Sensor: {
      const auto& s = record.sensor;
      return std::snprintf(line, capacity,
        "\nTW1,S,%" PRIu32 ",%" PRIu32 ",%" PRId64 ",%" PRIu32 ",%d,%d,%d,"
        "%" PRIu32 ",%d,%d,%.3f,%.3f,%.3f,%.3f,%.4f,%.3f,%.4f,%.4f,%d,%.3f,%.3f,%d,%d\n",
        bootId, record.sequence, record.uptime_us, s.sensor_ms, s.mode, s.pas_position,
        s.pedal_direction, s.pas_age_ms, s.torque_adc, s.torque_zero_adc, s.torque_nm,
        s.filtered_torque_nm, s.cadence_rpm, s.rider_w, s.assist_factor, s.assist_w,
        s.target_a, s.commanded_a, s.motor_enabled, s.speed_kmh, s.motor_rpm, s.speed_valid,
        s.command_published);
    }
    case RecordKind::Command: {
      const auto& c = record.command;
      return std::snprintf(line, capacity,
        "\nTW1,C,%" PRIu32 ",%" PRIu32 ",%" PRId64 ",%" PRIu32 ",%" PRIu32 ",%.4f,%" PRIu32 "\n",
        bootId, record.sequence, record.uptime_us, c.sensor_seq, c.sensor_ms, c.current_a,
        c.write_us);
    }
    case RecordKind::Vesc: {
      const auto& v = record.vesc;
      return std::snprintf(line, capacity,
        "\nTW1,V,%" PRIu32 ",%" PRIu32 ",%" PRId64 ",%" PRIu32 ",%d,%d,"
        "%.3f,%.3f,%.4f,%.4f,%.3f,%.5f,%.3f,%.4f,%.4f,%d\n",
        bootId, record.sequence, record.uptime_us, v.query_us, v.reply, v.speed_valid,
        v.speed_kmh, v.erpm, v.motor_a, v.battery_a, v.battery_v, v.duty, v.fet_c,
        v.watt_hours, v.watt_hours_charged, v.fault);
    }
  }
  return 0;
}

void rideTraceTask(void*)
{
  const uint32_t bootId = rideBootId();
  const int resetReason = rideResetReason();
  char line[LINE_CAPACITY];
  TraceRecord record;
  int64_t nextBootMarker = 0;

  writeTraceLine(BOOT_HEADER, sizeof(BOOT_HEADER) - 1);
  writeTraceLine(SENSOR_HEADER, sizeof(SENSOR_HEADER) - 1);
  writeTraceLine(COMMAND_HEADER, sizeof(COMMAND_HEADER) - 1);
  writeTraceLine(VESC_HEADER, sizeof(VESC_HEADER) - 1);

  for (;;) {
    const int64_t now = esp_timer_get_time();
    if (now >= nextBootMarker) {
      const int length = std::snprintf(line, sizeof(line), "\nTW1,B,%" PRIu32 ",%d,%" PRId64 "\n",
        bootId, resetReason, now);
      if (length > 0 && static_cast<size_t>(length) < sizeof(line)) {
        writeTraceLine(line, length);
      }
      nextBootMarker = now + BOOT_INTERVAL_US;
    }

    if (xQueueReceive(traceQueue, &record, pdMS_TO_TICKS(1000)) == pdTRUE) {
      const int length = formatRecord(line, sizeof(line), bootId, record);
      if (length > 0 && static_cast<size_t>(length) < sizeof(line)) {
        writeTraceLine(line, length);
      }
    }
  }
}

} // namespace

void setupRideTrace()
{
  traceQueue = xQueueCreateStatic(QUEUE_DEPTH, sizeof(TraceRecord), queueStorage, &queueControl);
  if (xTaskCreatePinnedToCore(rideTraceTask, "usbTrace", 4096, nullptr, tskIDLE_PRIORITY,
                             nullptr, 1) != pdPASS) {
    traceQueue = nullptr;
    Serial.println("# TW1 USB trace task could not start");
  }
}

void traceRideSensor(const RideSensorSample& sample)
{
  if (!traceQueue) return;
  TraceRecord record{RecordKind::Sensor, sample.sensor_seq, esp_timer_get_time(), {sample}};
  xQueueSend(traceQueue, &record, 0);
}

void traceRideCommand(const RideCommandSample& sample)
{
  if (!traceQueue) return;
  static uint32_t sequence = 0;
  TraceRecord record{};
  record.kind = RecordKind::Command;
  record.sequence = sequence++;
  record.uptime_us = esp_timer_get_time();
  record.command = sample;
  xQueueSend(traceQueue, &record, 0);
}

void traceRideVesc(const RideVescSample& sample)
{
  if (!traceQueue) return;
  static uint32_t sequence = 0;
  TraceRecord record{};
  record.kind = RecordKind::Vesc;
  record.sequence = sequence++;
  record.uptime_us = esp_timer_get_time();
  record.vesc = sample;
  xQueueSend(traceQueue, &record, 0);
}
#endif
