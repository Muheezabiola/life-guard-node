#pragma once

// Local web portal, served in safe mode only: when the cloud link
// cannot be established, the readings and the reset button stay
// reachable from a phone on the same network. Everything is a
// read-only GET apart from the single reset endpoint.
void webPortalBegin();

// Serves pending clients. Called once per loop iteration while the
// node is in safe mode.
void webPortalService();
