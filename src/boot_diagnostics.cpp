#include "boot_diagnostics.h"
#include <esp_system.h>

uint32_t rideBootId()
{
  static const uint32_t id = esp_random();
  return id;
}

int rideResetReason()
{
  return esp_reset_reason();
}
