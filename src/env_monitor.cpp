#include "env_monitor.h"

#include <Arduino.h>
#include <DHT.h>

#include "config.h"

// ============================================================
//  Station environment: DHT11 air sensor and analog gas probe
//
//  The two range predicates are deliberately not each other's
//  negation. The alert window is wide and the clear window is
//  narrow, so a reading that hovers around a threshold settles on
//  one answer instead of flapping.
//
//  Humidity is only ever a trigger, never a reason to clear: the
//  original node cleared on gas and temperature alone, and this
//  keeps that.
// ============================================================

namespace
{
DHT g_dht(PIN_DHT, DHT_MODEL);

int g_gasPercent = 0;
float g_temperatureC = NAN;
float g_humidityPct = NAN;
bool g_haveReading = false;
unsigned long g_lastDhtReadMs = 0;
} // namespace

void envMonitorInit()
{
    g_dht.begin();
}

void envMonitorPoll()
{
    g_gasPercent = map(analogRead(PIN_GAS), GAS_ADC_MIN, GAS_ADC_MAX, 0, GAS_SCALE_MAX_PCT);

    if (g_haveReading && millis() - g_lastDhtReadMs < DHT_REFRESH_MS)
    {
        return;
    }

    g_lastDhtReadMs = millis();
    g_haveReading = true;

    // A DHT11 simply fails to answer now and then and reports NaN.
    // That is kept rather than turned into a number: every
    // comparison against NaN is false, so a silent sensor reads as
    // "no evidence of a problem" instead of as a good reading, and
    // the screens and the JSON API render it as "--" and null.
    g_temperatureC = g_dht.readTemperature();
    g_humidityPct = g_dht.readHumidity();
}

int envMonitorGasPercent()
{
    return g_gasPercent;
}

float envMonitorTemperatureC()
{
    return g_temperatureC;
}

float envMonitorHumidityPct()
{
    return g_humidityPct;
}

bool envMonitorOutOfRange()
{
    return g_gasPercent > GAS_ALERT_PCT
        || g_temperatureC > AIR_TEMP_ALERT_C
        || g_humidityPct > HUMIDITY_ALERT_HIGH_PCT
        || g_humidityPct < HUMIDITY_ALERT_LOW_PCT;
}

bool envMonitorBackInRange()
{
    return g_gasPercent < GAS_CLEAR_PCT && g_temperatureC < AIR_TEMP_CLEAR_C;
}
