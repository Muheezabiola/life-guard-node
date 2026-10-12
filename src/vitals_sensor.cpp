#include "vitals_sensor.h"

#include <Wire.h>

#include <MAX30105.h>
#include <heartRate.h>
#include <spo2_algorithm.h>

#include "cloud_sync.h"
#include "config.h"

// ============================================================
//  Vitals: MAX30105 pulse oximeter
//
//  Maxim's algorithm wants a full second of samples, and the
//  original node kept refilling the buffer for ten seconds before
//  analysing it. That window is kept: the extra passes cost nothing
//  but time on a node that is idle between readings, and a finger
//  that shifts during the reading is then far more likely to have
//  settled by the time the analysed buffer is filled.
//
//  The sensor sleeps except while a reading is being taken, so its
//  LEDs are not left running (and heating the skin they are trying
//  to measure) between measurements.
// ============================================================

namespace
{
MAX30105 g_sensor;

uint32_t g_irBuffer[VITALS_BUFFER_SIZE];
uint32_t g_redBuffer[VITALS_BUFFER_SIZE];

int g_lastBpm = VITALS_DEFAULT_BPM;
int g_lastSpo2 = VITALS_DEFAULT_SPO2_PCT;
} // namespace

void vitalsSensorInit()
{
    g_sensor.begin(Wire, I2C_CLOCK_HZ);
    g_sensor.shutDown();
}

void vitalsSensorStart()
{
    g_sensor.wakeUp();
    g_sensor.setup(VITALS_LED_BRIGHTNESS, VITALS_SAMPLE_AVERAGE, VITALS_LED_MODE,
                   VITALS_SAMPLE_RATE, VITALS_PULSE_WIDTH, VITALS_ADC_RANGE);
}

void vitalsSensorStop()
{
    g_sensor.shutDown();
}

void vitalsSensorMeasure()
{
    const unsigned long startMs = millis();

    while (millis() - startMs < VITALS_ACQUIRE_WINDOW_MS)
    {
        for (uint8_t i = 0; i < VITALS_BUFFER_SIZE; i++)
        {
            while (!g_sensor.available())
            {
                g_sensor.check();
            }

            g_redBuffer[i] = g_sensor.getRed();
            g_irBuffer[i] = g_sensor.getIR();
            g_sensor.nextSample();
        }
    }

    int32_t spo2Value = 0;
    int32_t bpmValue = 0;
    int8_t spo2Valid = 0;
    int8_t bpmValid = 0;

    maxim_heart_rate_and_oxygen_saturation(g_irBuffer, VITALS_BUFFER_SIZE, g_redBuffer,
                                          &spo2Value, &spo2Valid, &bpmValue, &bpmValid);

    // A rejected reading falls back to the last known values instead
    // of publishing a zero, as the original node did: one bad
    // reading should not look like a patient in cardiac arrest.
    g_lastBpm = (bpmValid == 1 && bpmValue > VITALS_MIN_VALID_BPM) ? bpmValue : VITALS_DEFAULT_BPM;
    g_lastSpo2 = (spo2Valid == 1) ? spo2Value : VITALS_DEFAULT_SPO2_PCT;

    cloudSyncPublishVitals(g_lastBpm, g_lastSpo2);
}

int vitalsSensorLastBpm()
{
    return g_lastBpm;
}

int vitalsSensorLastSpo2()
{
    return g_lastSpo2;
}
