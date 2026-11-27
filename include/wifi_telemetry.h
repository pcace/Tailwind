#ifndef WIFI_TELEMETRY_H
#define WIFI_TELEMETRY_H

#include <Arduino.h>

// Kept for callers that formerly logged to the web interface.
#ifdef TAILWIND_USB_LOGGING
inline void addLogMessage(const String &) {}
inline void addLogMessage(const char *) {}
#else
void setupWifiTelemetry();
void addLogMessage(const String &message);
void addLogMessage(const char *message);
#endif

#endif // WIFI_TELEMETRY_H
