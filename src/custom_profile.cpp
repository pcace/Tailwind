#include "tailwind_controller.h"
#include <Preferences.h>
#include <algorithm>
#include <mutex>

namespace {
constexpr char preferences_namespace[] = "tailwind";
constexpr char curve_key[] = "assist-curve";
constexpr CustomAssistCurve default_curve{{60, 40, 30, 35, 45, 65, 85}};

CustomAssistCurve active_curve = default_curve;
portMUX_TYPE curve_mux = portMUX_INITIALIZER_UNLOCKED;
std::mutex persistence_mutex;

bool validCurve(const CustomAssistCurve &curve) {
  return std::all_of(curve.begin(), curve.end(), [](uint8_t gain) {
    return gain <= MAX_CUSTOM_GAIN_TENTHS;
  });
}

void publishCurve(const CustomAssistCurve &curve) {
  portENTER_CRITICAL(&curve_mux);
  active_curve = curve;
  portEXIT_CRITICAL(&curve_mux);
}
}

CustomAssistCurve getCustomAssistCurve() {
  portENTER_CRITICAL(&curve_mux);
  const CustomAssistCurve snapshot = active_curve;
  portEXIT_CRITICAL(&curve_mux);
  return snapshot;
}

void loadCustomAssistCurve() {
  const std::lock_guard<std::mutex> persistence_guard(persistence_mutex);
  Preferences preferences;
  CustomAssistCurve saved{};
  if (preferences.begin(preferences_namespace, true) &&
      preferences.isKey(curve_key) &&
      preferences.getBytes(curve_key, saved.data(), saved.size()) == saved.size() &&
      validCurve(saved)) {
    publishCurve(saved);
  } else {
    publishCurve(default_curve);
  }
}

bool saveCustomAssistCurve(const CustomAssistCurve &curve) {
  if (!validCurve(curve)) {
    return false;
  }

  // Serialize flash writes independently of the short sensor snapshot lock.
  const std::lock_guard<std::mutex> persistence_guard(persistence_mutex);
  if (curve == getCustomAssistCurve()) {
    return true;
  }

  Preferences preferences;
  if (!preferences.begin(preferences_namespace) ||
      preferences.putBytes(curve_key, curve.data(), curve.size()) != curve.size()) {
    return false;
  }

  publishCurve(curve);
  return true;
}
