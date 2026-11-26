#ifndef WIFI_TELEMETRY_H
#define WIFI_TELEMETRY_H

#include <Arduino.h>

void setupWifiTelemetry();

// Kept for callers that formerly logged to the web interface.
void addLogMessage(const String &message);
void addLogMessage(const char *message);

#endif // WIFI_TELEMETRY_H
