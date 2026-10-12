#include "alarm.h"

#include "config.h"

// ============================================================
//  Alert outputs: relay, buzzer and status LED
//
//  The relay is the station's outlet, energised for as long as an
//  alert is up — the behaviour the original node had. The buzzer
//  and the status LED pulse rather than sound continuously, which
//  stays audible from outside the room without drowning it out.
//
//  The pulse holds its own timestamp, and the interval arithmetic
//  is unsigned, so the pattern survives the millis() rollover after
//  ~49 days of uptime.
// ============================================================

namespace
{
bool g_pulseActive = false;
unsigned long g_lastPulseMs = 0;

void setRelay(bool energised)
{
    digitalWrite(PIN_RELAY, energised ? HIGH : LOW);
}
} // namespace

void alarmInit()
{
    pinMode(PIN_RELAY, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_STATUS_LED, OUTPUT);

    setRelay(false);
    alarmSilence();
}

void alarmSilence()
{
    noTone(PIN_BUZZER);
    digitalWrite(PIN_STATUS_LED, LOW);
    g_pulseActive = false;
}

void alarmToneOff()
{
    noTone(PIN_BUZZER);
}

void alarmPulse()
{
    if (millis() - g_lastPulseMs <= ALERT_BEEP_INTERVAL_MS)
    {
        return;
    }

    g_lastPulseMs = millis();
    g_pulseActive = !g_pulseActive;

    if (g_pulseActive)
    {
        tone(PIN_BUZZER, ALERT_TONE_HZ);
        digitalWrite(PIN_STATUS_LED, HIGH);
    }
    else
    {
        alarmSilence();
    }
}

void alarmServiceAlert(bool alertActive)
{
    setRelay(alertActive);

    if (alertActive)
    {
        alarmPulse();
    }
}
