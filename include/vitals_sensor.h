#pragma once

// Vitals: heart rate and blood oxygen from a MAX30105 pulse
// oximeter.
//
// A measurement blocks the node for VITALS_ACQUIRE_WINDOW_MS while
// it fills the sample buffer, then runs Maxim's algorithm over the
// last buffer. A reading the algorithm rejects falls back to the
// last known values rather than publishing a zero.
void vitalsSensorInit();

// Wake the sensor and apply the sampling configuration.
void vitalsSensorStart();

// Put the sensor back to sleep.
void vitalsSensorStop();

// Sample for the acquisition window, then store and publish the
// result.
void vitalsSensorMeasure();

int vitalsSensorLastBpm();
int vitalsSensorLastSpo2();
