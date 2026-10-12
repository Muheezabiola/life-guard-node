#pragma once

// Alert layer.
//
// Two independent sources raise an alert: the pipeline node over
// ESP-NOW and this node's own environment readings. This module is
// the single place that turns either of them into the relay, the
// buzzer and status LED, the LCD banner and the WhatsApp
// notification, so the two sources can never disagree about what is
// happening.
void alertManagerInit();

// Enables (or gates off) both alert sources. The node enables them
// once its boot sequence is complete.
void alertManagerEnable(bool enabled);

// One pass: update the alert states, drive the outputs and send any
// notification that a rising edge earned.
void alertManagerUpdate();

// True while an alert is up.
bool alertManagerActive();

// Clears both alert states, releases the relay, silences the buzzer
// and tells the pipeline node the situation is acknowledged. Wired
// to the portal's reset button and to the remote reset flag in the
// database.
void alertManagerClear();
