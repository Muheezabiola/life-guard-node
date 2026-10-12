#include "whatsapp.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include "alarm.h"
#include "cloud_sync.h"
#include "config.h"

// ============================================================
//  WhatsApp notifications
//
//  One cooldown per category, so a fault that stays active does not
//  turn into a message every loop pass. The cooldown only starts
//  once the gateway has answered, so a failed send is retried.
//
//  The request is a blocking GET on the station's Wi-Fi link, which
//  is why the alert tone is switched off first: a tone that keeps
//  sounding while nothing services it would run for as long as the
//  request takes.
// ============================================================

namespace
{
unsigned long g_lastSentMs[WHATSAPP_CATEGORY_COUNT] = { 0 };

String urlEncode(const String& value)
{
    String encoded;
    encoded.reserve(value.length() * 3);

    for (size_t i = 0; i < value.length(); i++)
    {
        const uint8_t c = static_cast<uint8_t>(value.charAt(i));

        if (isalnum(c))
        {
            encoded += static_cast<char>(c);
        }
        else if (c == ' ')
        {
            encoded += '+';
        }
        else
        {
            encoded += '%';
            if (c < 16)
            {
                encoded += '0';
            }
            encoded += String(c, HEX);
        }
    }

    return encoded;
}
} // namespace

bool whatsappSend(const String& message, WhatsappCategory category)
{
    // Notifications only make sense while the cloud link is up, which
    // is exactly what safe mode is the absence of.
    if (!cloudSyncReady() || WiFi.status() != WL_CONNECTED)
    {
        return false;
    }

    const unsigned long now = millis();
    const unsigned long lastSent = g_lastSentMs[category];

    if (lastSent != 0 && now - lastSent < WHATSAPP_COOLDOWN_MS)
    {
        Serial.printf("[whatsapp] category %d still in cooldown\n", static_cast<int>(category));
        return false;
    }

    alarmToneOff();

    WiFiClient client;
    HTTPClient http;
    const String url = String(WHATSAPP_BASE_URL) + "apikey=" + WHATSAPP_API_KEY
        + "&text=" + urlEncode(message) + "&phone=" + WHATSAPP_PHONE;

    http.begin(client, url);
    http.setTimeout(WHATSAPP_HTTP_TIMEOUT_MS);
    const int status = http.GET();
    http.end();

    if (status != 200)
    {
        Serial.printf("[whatsapp] gateway answered %d\n", status);
        return false;
    }

    g_lastSentMs[category] = now;
    Serial.println("[whatsapp] notification sent");
    return true;
}
