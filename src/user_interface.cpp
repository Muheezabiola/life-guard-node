#include "user_interface.h"

#include "alarm.h"
#include "body_temp.h"
#include "config.h"
#include "env_monitor.h"
#include "lcd_display.h"
#include "vitals_sensor.h"

// ============================================================
//  Operating modes and the buttons that select them
//
//  Mode 0 is the node doing its job: it mirrors the station
//  environment. Modes 1 and 2 are what the person standing at the
//  station asked for, and they run in a chain — a contactless
//  temperature, then the vitals reading that goes with it.
//
//  Only one screen owns the display at a time, and only while no
//  alert is up: the alert banner takes precedence, which is why the
//  caller checks alertManagerActive() before coming in here.
// ============================================================

namespace
{
uint8_t g_mode = MODE_ENVIRONMENT;
bool g_feverScreenDrawn = false;
unsigned long g_lastScreenMs = 0;

bool buttonPressed(uint8_t pin)
{
    return digitalRead(pin) == LOW; // Wired to ground behind a pull-up
}

void confirmBeep()
{
    tone(PIN_BUZZER, UI_TONE_HZ, UI_TONE_MS);
}

// Renders a reading, or "--" when the sensor did not answer: NaN
// cast to an integer prints as garbage, and a garbage number on a
// health monitor is worse than a visible gap.
void formatReading(char* buffer, size_t bufferSize, float value, uint8_t decimals)
{
    if (isnan(value))
    {
        snprintf(buffer, bufferSize, "--");
        return;
    }

    snprintf(buffer, bufferSize, "%.*f", decimals, value);
}

void switchMode()
{
    confirmBeep();

    g_mode = (g_mode == MODE_ENVIRONMENT) ? MODE_BODY_TEMP : MODE_ENVIRONMENT;
    bodyTempClear();
    g_feverScreenDrawn = false;
    lcdDisplayClear();
    alarmSilence();
}

void showEnvironmentScreen()
{
    if (millis() - g_lastScreenMs < LCD_REFRESH_MS)
    {
        return;
    }

    g_lastScreenMs = millis();

    char humidity[8];
    char temperature[8];
    formatReading(humidity, sizeof(humidity), envMonitorHumidityPct(), 0);
    formatReading(temperature, sizeof(temperature), envMonitorTemperatureC(), 0);

    char line[LCD_COLUMNS + 1];
    snprintf(line, sizeof(line), "G:%d H:%s%% T:%s", envMonitorGasPercent(), humidity, temperature);
    lcdDisplayWrite(0, line);

    const bool outOfRange = envMonitorGasPercent() > GAS_ALERT_PCT
        || envMonitorTemperatureC() > AIR_TEMP_ALERT_C;
    lcdDisplayWrite(1, outOfRange ? "STATUS: BAD" : "STATUS: GOOD");
}

void updateBodyTempMode()
{
    if (!bodyTempCaptured())
    {
        char live[8];
        formatReading(live, sizeof(live), bodyTempLiveC(), BODY_TEMP_DECIMALS);

        lcdDisplayWrite(0, "SCAN TEMP...");
        lcdDisplayWrite(1, String("LIVE: ") + live + "C");

        // A capture only counts when the sensor is looking at
        // something warmer than the room.
        if (buttonPressed(PIN_BUTTON_VITALS) && bodyTempTryCapture())
        {
            confirmBeep();
            lcdDisplayClear();
            delay(MODE_SWITCH_GUARD_MS);
        }

        return;
    }

    if (bodyTempIsFever())
    {
        if (!g_feverScreenDrawn)
        {
            lcdDisplayShow("HIGH FEVER ALERT", "VISIT HOSPITAL");
            g_feverScreenDrawn = true;
        }

        alarmPulse();

        if (buttonPressed(PIN_BUTTON_VITALS))
        {
            g_mode = MODE_ENVIRONMENT;
            bodyTempClear();
            g_feverScreenDrawn = false;
            lcdDisplayClear();
            alarmSilence();
        }

        return;
    }

    char held[8];
    formatReading(held, sizeof(held), bodyTempCapturedC(), BODY_TEMP_DECIMALS);

    lcdDisplayWrite(0, "TEMP CAPTURED");
    lcdDisplayWrite(1, String("HOLD:") + held + "C NEXT>");

    if (buttonPressed(PIN_BUTTON_VITALS))
    {
        confirmBeep();
        g_mode = MODE_VITALS;
        lcdDisplayClear();
        vitalsSensorStart();
    }
}

void updateVitalsMode()
{
    lcdDisplayShow("PLACE FINGER ON", "THE BPM SENSOR");

    vitalsSensorMeasure();

    char held[8];
    formatReading(held, sizeof(held), bodyTempCapturedC(), BODY_TEMP_DECIMALS);

    lcdDisplayShow(String("HR:") + vitalsSensorLastBpm() + " SpO2:" + vitalsSensorLastSpo2() + "%",
                   String("TEMP: ") + held + "C (OK)");

    delay(VITALS_HOLD_MS);

    vitalsSensorStop();
    g_mode = MODE_ENVIRONMENT;
    bodyTempClear();
    lcdDisplayClear();
    alarmSilence();
}
} // namespace

void userInterfaceInit()
{
    pinMode(PIN_BUTTON_MODE, INPUT_PULLUP);
    pinMode(PIN_BUTTON_VITALS, INPUT_PULLUP);
}

void userInterfaceUpdate()
{
    // The mode button is read first and the selected mode then runs
    // in the same pass, which is how the original behaved: switching
    // back to the environment screen redraws it immediately.
    if (buttonPressed(PIN_BUTTON_MODE))
    {
        switchMode();
    }

    switch (g_mode)
    {
    case MODE_BODY_TEMP:
        updateBodyTempMode();
        break;
    case MODE_VITALS:
        updateVitalsMode();
        break;
    case MODE_ENVIRONMENT:
    default:
        showEnvironmentScreen();
        break;
    }
}
