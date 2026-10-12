#pragma once

// ============================================================
//  CREDENTIALS TEMPLATE
//
//  Copy this file, then fill in the real values:
//
//      copy include\secrets.example.h include\secrets.h
//
//  include/secrets.h is listed in .gitignore and must never be
//  committed or pushed to GitHub. The build stops with a clear
//  #error while it is missing (see include/config.h).
//
//  Every value below belongs to an account that can be used to
//  impersonate the node, so treat this file as the only copy: it is
//  not a place to keep a project's credentials alongside the code.
// ============================================================

// --- Wi-Fi network the node joins at boot (2.4 GHz only) ---
#define WIFI_SSID "your-network"
#define WIFI_PASSWORD "your-password"

// --- Firebase project: web API key plus an email/password user ---
// The database rules should restrict that user to this node's
// paths; an API key is not a secret on its own, but the account
// behind it is.
#define FIREBASE_API_KEY "your-web-api-key"
#define FIREBASE_USER_EMAIL "node@example.com"
#define FIREBASE_USER_PASSWORD "your-firebase-user-password"
#define FIREBASE_DATABASE_URL "https://your-project-default-rtdb.firebaseio.com"

// --- WhatsApp gateway ---
// WHATSAPP_PHONE is the number that receives the notifications;
// WHATSAPP_API_KEY is issued by the gateway.
#define WHATSAPP_PHONE "2348000000000"
#define WHATSAPP_API_KEY "your-gateway-api-key"
#define WHATSAPP_BASE_URL "http://api.whatabot.net/whatsapp/sendMessage?"
