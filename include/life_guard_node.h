#pragma once

// Node flow: everything the LifeGuard node does, in order.
//
// lifeGuardNodeInit()   boot: display, Wi-Fi, cloud, sensors,
//                       pipeline link and — if the cloud did not
//                       answer — the local portal
// lifeGuardNodeUpdate() one pass: serve the network, sample, raise
//                       or clear alerts, sync the cloud, then hand
//                       the display to the selected mode
void lifeGuardNodeInit();
void lifeGuardNodeUpdate();
