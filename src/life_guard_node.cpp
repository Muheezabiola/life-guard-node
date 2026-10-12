#include "life_guard_node.h"

#include <WiFi.h>
#include <Wire.h>

#include "alert_manager.h"
#include "body_temp.h"
#include "cloud_sync.h"
#include "config.h"
#include "env_monitor.h"
#include "lcd_display.h"
#include "pipe_link.h"
#include "user_interface.h"
#include "vitals_sensor.h"
#include "web_portal.h"

// ============================================================
//  LifeGuard Node — the node's own flow
//
//  Boot, in order:
//    alert outputs, display, Wi-Fi, cloud sign-in, then — if the
//    cloud did not answer — the local portal, followed by the
//    pipeline link, the sensors and the buttons. Alerts stay gated
//    until the very end, so a message that arrives while the node is
//    still starting up is mirrored but does not raise one.
//
//  One pass of loop(), in order:
//    serve whichever network is active (portal in safe mode, cloud
//    client otherwise), sample the station, update the alert layer,
//    publish on the sync interval, then hand the display to the
//    selected mode — but only while no alert is up.
// ============================================================

namespace
{
bool g_safeMode = false;

void connectToWifi()
{
    lcdDisplayShow("LIFEGUARD NODE", "CONNECTING WIFI");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // The dots are the only sign of life during the association, so
    // the row keeps them until it is full.
    String progress = "CONNECTING WIFI";
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print('.');
        delay(WIFI_RETRY_INTERVAL_MS);

        if (progress.length() < LCD_COLUMNS)
        {
            progress += '.';
            lcdDisplayWrite(1, progress);
        }
    }

    Serial.printf("\n[wifi] connected, ip %s\n", WiFi.localIP().toString().c_str());
}

void syncCloud()
{
    if (!cloudSyncDueForPublish())
    {
        return;
    }

    if (cloudSyncTakeRemoteReset())
    {
        Serial.println("[cloud] remote reset requested");
        alertManagerClear();
    }

    cloudSyncPublishSituation(pipeLinkSituation());
    cloudSyncPublishEnvironment(envMonitorGasPercent(), envMonitorTemperatureC(), envMonitorHumidityPct());
}
} // namespace

void lifeGuardNodeInit()
{
    Serial.begin(SERIAL_BAUD);
    Serial.println("\n[LifeGuardNode] starting up");

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_CLOCK_HZ);

    alertManagerInit();
    lcdDisplayInit();
    connectToWifi();

    lcdDisplayClear();
    lcdDisplayWrite(0, "CLOUD SYNCING");
    cloudSyncBegin();

    g_safeMode = !cloudSyncReady();

    if (g_safeMode)
    {
        lcdDisplayShow("SAFE MODE: ON", WiFi.localIP().toString());
        webPortalBegin();
        delay(SAFE_MODE_HOLD_MS);
    }
    else
    {
        cloudSyncPublishStatusOnline();
    }

    pipeLinkInit();

    envMonitorInit();
    bodyTempInit();
    vitalsSensorInit();
    userInterfaceInit();

    alertManagerEnable(true);

    lcdDisplayShow("SYSTEM READY", "");
    Serial.println("[LifeGuardNode] ready");
}

void lifeGuardNodeUpdate()
{
    if (g_safeMode)
    {
        webPortalService();
    }
    else
    {
        cloudSyncPoll();
    }

    envMonitorPoll();
    alertManagerUpdate();
    syncCloud();

    if (!alertManagerActive())
    {
        userInterfaceUpdate();
    }
}
