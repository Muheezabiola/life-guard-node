#pragma once

#include <Arduino.h>

// WhatsApp notifications through the gateway in include/secrets.h.
//
// WhatsappCategory exists so that the cooldown is per source: a
// noisy environment reading does not swallow the pipeline alert
// that arrives a minute later.
enum WhatsappCategory
{
    WHATSAPP_CATEGORY_PIPELINE = 0,
    WHATSAPP_CATEGORY_ENVIRONMENT,
    WHATSAPP_CATEGORY_COUNT
};

// Sends one notification. Returns false without sending anything if
// the cloud link is not up or the category is still in its cooldown.
bool whatsappSend(const String& message, WhatsappCategory category);
