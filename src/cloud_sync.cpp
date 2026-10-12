// The library is configured by these two defines, which have to be
// set before its header is included: user (email/password)
// authentication, and the Realtime Database.
#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include "cloud_sync.h"

#include <FirebaseClient.h>
#include <WiFiClientSecure.h>

#include "config.h"

// ============================================================
//  Cloud link: Firebase Realtime Database
//
//  The node signs in as an email/password user and then publishes
//  on a timer. Everything here is a no-op until that sign-in has
//  succeeded, which is what safe mode is the absence of.
//
//  The certificate chain is not verified: the ESP32 has no clock it
//  can trust before its first sync, so a pinned root would fail
//  exactly when the node needs it. The traffic is still encrypted —
//  see the design notes in README.md.
// ============================================================

namespace
{
WiFiClientSecure g_sslClient;
AsyncClientClass g_asyncClient(g_sslClient);
FirebaseApp g_app;
RealtimeDatabase g_database;
AsyncResult g_authResult;
UserAuth g_userAuth(FIREBASE_API_KEY, FIREBASE_USER_EMAIL, FIREBASE_USER_PASSWORD);

bool g_ready = false;
unsigned long g_lastPublishMs = 0;
} // namespace

void cloudSyncBegin()
{
    g_sslClient.setInsecure();

    initializeApp(g_asyncClient, g_app, getAuth(g_userAuth), g_authResult);
    g_app.getApp<RealtimeDatabase>(g_database);
    g_database.url(FIREBASE_DATABASE_URL);

    const unsigned long startMs = millis();
    while (!g_app.ready() && millis() - startMs < CLOUD_READY_TIMEOUT_MS)
    {
        g_app.loop();
        delay(CLOUD_SYNC_SLICE_MS);
    }

    g_ready = g_app.ready();

    if (g_ready)
    {
        Serial.printf("[cloud] signed in as %s\n", FIREBASE_USER_EMAIL);
    }
    else
    {
        Serial.println("[cloud] not ready - local portal only");
    }
}

bool cloudSyncReady()
{
    return g_ready;
}

void cloudSyncPoll()
{
    if (g_ready)
    {
        g_app.loop();
    }
}

bool cloudSyncDueForPublish()
{
    if (!g_ready || !g_app.ready())
    {
        return false;
    }

    if (millis() - g_lastPublishMs < CLOUD_SYNC_INTERVAL_MS)
    {
        return false;
    }

    g_lastPublishMs = millis();
    return true;
}

bool cloudSyncTakeRemoteReset()
{
    if (!g_ready)
    {
        return false;
    }

    if (!g_database.get<bool>(g_asyncClient, CLOUD_PATH_REMOTE_RESET))
    {
        return false;
    }

    g_database.set<bool>(g_asyncClient, CLOUD_PATH_REMOTE_RESET, false);
    return true;
}

void cloudSyncPublishStatusOnline()
{
    if (!g_ready)
    {
        return;
    }

    g_database.set<String>(g_asyncClient, CLOUD_PATH_STATUS, CLOUD_STATUS_ONLINE);
}

void cloudSyncPublishSituation(const char* situation)
{
    if (!g_ready)
    {
        return;
    }

    g_database.set<String>(g_asyncClient, CLOUD_PATH_SITUATION, situation);
}

void cloudSyncPublishEnvironment(int gasPercent, float temperatureC, float humidityPct)
{
    if (!g_ready)
    {
        return;
    }

    g_database.set<int>(g_asyncClient, CLOUD_PATH_GAS, gasPercent);

    // A reading the sensor did not produce is left out rather than
    // published as a number that is not one: the database then keeps
    // the last good value instead of losing it.
    if (!isnan(temperatureC))
    {
        g_database.set<number_t>(g_asyncClient, CLOUD_PATH_TEMP, number_t(temperatureC, CLOUD_TEMP_DECIMALS));
    }

    if (!isnan(humidityPct))
    {
        g_database.set<number_t>(g_asyncClient, CLOUD_PATH_HUMIDITY, number_t(humidityPct, CLOUD_TEMP_DECIMALS));
    }
}

void cloudSyncPublishVitals(int bpm, int spo2Pct)
{
    if (!g_ready)
    {
        return;
    }

    g_database.set<int>(g_asyncClient, CLOUD_PATH_LAST_BPM, bpm);
    g_database.set<int>(g_asyncClient, CLOUD_PATH_LAST_SPO2, spo2Pct);
}
