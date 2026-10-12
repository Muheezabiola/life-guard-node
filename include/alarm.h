#pragma once

// Alert outputs: the relay that cuts the station outlet, the buzzer
// and the status LED. These are the hardware side of an alert; what
// raises one is decided in alert_manager.
void alarmInit();

// Tone off and status LED off.
void alarmSilence();

// Tone off only, leaving the status LED alone. Used before a
// blocking HTTP request, where a tone that keeps running would be
// driven by nothing for seconds.
void alarmToneOff();

// One toggle of the alert pulse (buzzer + status LED). Call it
// repeatedly: it keeps its own interval.
void alarmPulse();

// Relay follows the alert state, buzzer and LED pulse while it is
// raised. A cleared alert only releases the relay — silence is
// alarmSilence(), which the callers that own the alert decide.
void alarmServiceAlert(bool alertActive);
