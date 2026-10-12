#pragma once

// Operating modes and the buttons that select them.
//
// The node is a monitor first: mode 0 shows the station
// environment, mode 1 takes a contactless temperature, mode 2 takes
// a heart rate and SpO2 reading. Button 1 switches between the two
// first modes; button 2 is the "capture / next step" action inside
// the two health modes.
void userInterfaceInit();

// Runs the selected mode. The caller only reaches this while no
// system alert is up, because the alert banner owns the display
// then.
void userInterfaceUpdate();
