#include "alert_manager.h"

#include "alarm.h"
#include "config.h"
#include "env_monitor.h"
#include "lcd_display.h"
#include "pipe_link.h"
#include "whatsapp.h"

// ============================================================
//  Alert layer
//
//  Two sources raise an alert. The pipeline alert arrives from the
//  peer over ESP-NOW and is already a level: the peer reports a
//  situation and reports "Normal" when it is over. The environment
//  alert is derived here from this node's own readings, with the
//  hysteresis env_monitor describes.
//
//  Either one puts the node into the alert state, and that state is
//  the only thing that drives the relay, the buzzer, the status LED,
//  the banner on the display and the WhatsApp notification, so the
//  two sources can never disagree about what is happening.
//
//  A notification is sent on the rising edge only: a situation that
//  stays up does not repeat itself, and the per-category cooldown in
//  whatsapp covers a fault that clears and returns immediately.
// ============================================================

namespace
{
bool g_enabled = false;
bool g_active = false;

bool g_pipeAlert = false;    // Pipeline level as of the previous pass
bool g_envAlert = false;     // Environment level as of the previous pass
bool g_envAlertNow = false;  // Environment level this pass

unsigned long g_lastBannerMs = 0;

void showBanner()
{
    if (millis() - g_lastBannerMs < LCD_REFRESH_MS)
    {
        return;
    }

    g_lastBannerMs = millis();

    // The pipeline alert is the one that came in from elsewhere, so
    // it is the one named on screen when both are up.
    lcdDisplayWrite(0, "!! ALERT !!");
    lcdDisplayWrite(1, pipeLinkAlertActive() ? "PIPE ERROR" : "ENV ERROR");
}
} // namespace

void alertManagerInit()
{
    alarmInit();
}

void alertManagerEnable(bool enabled)
{
    g_enabled = enabled;
    pipeLinkEnableAlerts(enabled);
}

bool alertManagerActive()
{
    return g_active;
}

void alertManagerUpdate()
{
    // --- Environment alert: raised by any reading outside the wide
    //     window, cleared only once gas and temperature are inside
    //     the narrow one ---
    if (g_enabled)
    {
        if (envMonitorOutOfRange())
        {
            g_envAlertNow = true;
        }
        else if (g_envAlertNow && envMonitorBackInRange())
        {
            g_envAlertNow = false;
        }
    }

    const bool pipeAlertNow = pipeLinkAlertActive();
    const bool active = pipeAlertNow || g_envAlertNow;

    // --- One notification per rising edge ---
    if (g_enabled)
    {
        if (pipeAlertNow && !g_pipeAlert)
        {
            Serial.printf("[alert] pipeline: %s\n", pipeLinkSituation());
            whatsappSend(String("PIPELINE ALERT: ") + pipeLinkSituation(), WHATSAPP_CATEGORY_PIPELINE);
        }

        if (g_envAlertNow && !g_envAlert)
        {
            Serial.println("[alert] environment out of range");
            whatsappSend(String("ENV ALERT: Gas: ") + envMonitorGasPercent() + "% Temp: "
                             + String(envMonitorTemperatureC()) + "C",
                         WHATSAPP_CATEGORY_ENVIRONMENT);
        }
    }

    g_pipeAlert = pipeAlertNow;
    g_envAlert = g_envAlertNow;

    // --- Outputs ---
    if (active)
    {
        alarmServiceAlert(true);
        showBanner();
    }
    else
    {
        // Falling edge: the buzzer used to keep sounding once the
        // pipeline reported "Normal", so the alert layer silences it
        // when the last source lets go.
        if (g_active)
        {
            alarmSilence();
        }

        alarmServiceAlert(false);
    }

    g_active = active;
}

void alertManagerClear()
{
    pipeLinkClearAlert();
    g_pipeAlert = false;
    g_envAlert = false;
    g_envAlertNow = false;
    g_active = false;

    alarmSilence();
    pipeLinkSendCorrection();
    alarmServiceAlert(false);

    lcdDisplayShow("ALERT CLEARED", "");
    delay(RESET_MESSAGE_HOLD_MS);
}
