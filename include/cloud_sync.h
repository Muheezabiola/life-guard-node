#pragma once

// Cloud link: Firebase Realtime Database, signed in as an
// email/password user.
//
// cloudSyncBegin() waits for the client to be ready and records the
// outcome once. If it never becomes ready the node runs in safe
// mode: every function below becomes a no-op and the local web
// portal takes over instead (see web_portal.h).
void cloudSyncBegin();

// True when the node is signed in. This is what "not in safe mode"
// means everywhere else.
bool cloudSyncReady();

// Drives the client's async machinery. Called once per loop
// iteration while the node is not in safe mode.
void cloudSyncPoll();

// True every CLOUD_SYNC_INTERVAL_MS, and only while the client is
// ready: the caller then owns this publish slot.
bool cloudSyncDueForPublish();

// Reads the remote reset flag and clears it. True if it was set, in
// which case the caller clears the alerts.
bool cloudSyncTakeRemoteReset();

void cloudSyncPublishStatusOnline();
void cloudSyncPublishSituation(const char* situation);
void cloudSyncPublishEnvironment(int gasPercent, float temperatureC, float humidityPct);
void cloudSyncPublishVitals(int bpm, int spo2Pct);
