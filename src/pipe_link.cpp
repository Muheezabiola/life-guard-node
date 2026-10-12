#include "pipe_link.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "config.h"

// ============================================================
//  Pipeline link: one ESP-NOW peer
//
//  The pipeline node pushes a situation string; this node answers a
//  reset with the same structure and the "corrected" flag set, which
//  is how the peer learns the situation was dealt with.
//
//  ESP-NOW follows the Wi-Fi interface, so the radio is pinned to
//  the channel the station associated with before the link is
//  started. Both ends then have to be on that same channel — see
//  the design notes in README.md.
//
//  The link is unauthenticated: the peer is trusted by address, not
//  by key. Treat it as a local sensor link, not as a control
//  channel.
// ============================================================

namespace
{
const uint8_t g_peerMac[] = PIPE_PEER_MAC;

PipeMessage g_outgoing = { "Normal", true };

// The situation is mirrored into a NUL-terminated copy of its own:
// the wire field is exactly PIPE_SITUATION_LEN bytes and a peer is
// free to fill all of them.
char g_situation[PIPE_SITUATION_LEN + 1] = "Normal";

bool g_alertsEnabled = false;
bool g_alertActive = false;

// The callback signature is the ESP-NOW one in the ESP32 core this
// project builds against: the sender's address, the payload, its
// length. The peer is identified by address, so the address itself
// is not used here.
void onDataRecv(const uint8_t* macAddr, const uint8_t* incoming, int length)
{
    (void)macAddr;

    if (length != sizeof(PipeMessage))
    {
        // Not a payload this node understands; nothing to mirror.
        Serial.printf("[pipe] dropped %d-byte message\n", length);
        return;
    }

    PipeMessage message = {};
    memcpy(&message, incoming, sizeof(message));

    memcpy(g_situation, message.situation, PIPE_SITUATION_LEN);
    g_situation[PIPE_SITUATION_LEN] = '\0';

    if (strcmp(g_situation, "Normal") != 0)
    {
        if (g_alertsEnabled && !g_alertActive)
        {
            g_alertActive = true;
            Serial.printf("[pipe] alert: %s\n", g_situation);
        }
    }
    else
    {
        g_alertActive = false;
    }
}
} // namespace

void pipeLinkInit()
{
    const int32_t channel = WiFi.channel();
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("[pipe] esp-now init failed");
        return;
    }

    esp_now_register_recv_cb(onDataRecv);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, g_peerMac, sizeof(g_peerMac));
    peerInfo.channel = channel;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("[pipe] add peer failed");
        return;
    }

    Serial.printf("[pipe] listening on channel %d\n", channel);
}

void pipeLinkEnableAlerts(bool enabled)
{
    g_alertsEnabled = enabled;
}

const char* pipeLinkSituation()
{
    return g_situation;
}

bool pipeLinkAlertActive()
{
    return g_alertActive;
}

void pipeLinkClearAlert()
{
    g_alertActive = false;
}

void pipeLinkSendCorrection()
{
    g_outgoing.corrected = true;

    if (esp_now_send(g_peerMac, reinterpret_cast<uint8_t*>(&g_outgoing), sizeof(g_outgoing)) != ESP_OK)
    {
        Serial.println("[pipe] correction not acknowledged");
    }
}
