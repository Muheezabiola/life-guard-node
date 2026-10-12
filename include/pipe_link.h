#pragma once

#include <Arduino.h>

#include "config.h"

// Pipeline link: one ESP-NOW peer, the sensor node watching the
// pipeline itself.
//
// PipeMessage is a wire contract shared with that board. It must
// keep this layout — same field order, same sizes — or the two ends
// will read each other's bytes as something else.

struct PipeMessage
{
    char situation[PIPE_SITUATION_LEN]; // "Normal", or the fault the peer reports
    bool corrected;                     // acknowledgement this node sends back
};

// Starts ESP-NOW on the channel the Wi-Fi station is on and
// registers the peer.
void pipeLinkInit();

// Gates the alert state below. Until this is true, messages are
// still mirrored but no alert is raised — the node is still booting.
void pipeLinkEnableAlerts(bool enabled);

// Latest situation reported by the peer, always NUL-terminated.
const char* pipeLinkSituation();

bool pipeLinkAlertActive();

// Drops the alert without waiting for the peer to report "Normal".
void pipeLinkClearAlert();

// Tells the peer this node has dealt with the situation.
void pipeLinkSendCorrection();
