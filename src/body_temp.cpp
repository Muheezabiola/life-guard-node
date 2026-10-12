#include "body_temp.h"

#include <Adafruit_MLX90614.h>

#include "config.h"

// ============================================================
//  Contactless body temperature (MLX90614)
//
//  The sensor measures whatever its field of view happens to
//  contain, so a capture is only accepted above BODY_TEMP_MIN_C:
//  pointing it at an empty room returns room temperature, which
//  would otherwise be recorded as a patient reading.
//
//  The algorithm that turns the field of view into a body
//  temperature is the sensor's own, and it assumes the forehead
//  fills most of that view — see the design notes in README.md.
// ============================================================

namespace
{
Adafruit_MLX90614 g_mlx;
float g_capturedC = 0.0f;
bool g_captured = false;
} // namespace

void bodyTempInit()
{
    g_mlx.begin();
}

float bodyTempLiveC()
{
    return g_mlx.readObjectTempC();
}

bool bodyTempCaptured()
{
    return g_captured;
}

float bodyTempCapturedC()
{
    return g_capturedC;
}

bool bodyTempTryCapture()
{
    const float liveC = bodyTempLiveC();

    if (liveC <= BODY_TEMP_MIN_C)
    {
        return false;
    }

    g_capturedC = liveC;
    g_captured = true;
    return true;
}

void bodyTempClear()
{
    g_captured = false;
    g_capturedC = 0.0f;
}

bool bodyTempIsFever()
{
    return g_captured && g_capturedC >= BODY_FEVER_C;
}
