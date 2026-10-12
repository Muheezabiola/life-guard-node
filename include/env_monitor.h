#pragma once

// Station environment: the DHT11 air sensor and the analog gas
// probe. The module samples and range-checks; it holds no alarm
// state of its own.

// Configures the gas channel and starts the DHT11.
void envMonitorInit();

// Refreshes the cached readings. The DHT11 is read at most once
// every DHT_REFRESH_MS because it cannot answer faster than that;
// the gas channel is read on every call.
void envMonitorPoll();

int envMonitorGasPercent();
float envMonitorTemperatureC();
float envMonitorHumidityPct();

// True when any reading is outside the alert window, and true when
// gas and temperature are both back inside the narrow one. The two
// are not each other's negation: that hysteresis is deliberate and
// is what alert_manager acts on.
bool envMonitorOutOfRange();
bool envMonitorBackInRange();
