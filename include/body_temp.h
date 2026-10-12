#pragma once

// Contactless body temperature (MLX90614 on the I2C bus).
//
// The sensor streams a live reading; a capture is only accepted when
// that reading is high enough to be a forehead rather than an empty
// room, and the held value is what the fever screen, the LCD in
// vitals mode and the web portal report.
void bodyTempInit();

float bodyTempLiveC();

bool bodyTempCaptured();
float bodyTempCapturedC();

// Captures the live reading. Returns false when the reading is too
// low to be a person, in which case nothing is held.
bool bodyTempTryCapture();

void bodyTempClear();

// True when a captured reading is at or above BODY_FEVER_C.
bool bodyTempIsFever();
