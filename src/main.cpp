#include <Arduino.h>

#include "life_guard_node.h"

// ============================================================
//  LifeGuard Node — firmware entry point
//
//  This file stays a thin entry point: it hands control to the node
//  module, so no board-specific detail leaks in here.
//
//  boot / one pass of loop()
//    life_guard_node  the flow: network, cloud, alerts, modes
//    lcd_display      the 16x2 display behind one padding rule
//    alarm            relay, buzzer and status LED outputs
//    alert_manager    pipeline + environment alerts and their policy
//    pipe_link        ESP-NOW link to the pipeline node
//    env_monitor      DHT11 air sensor and gas probe
//    body_temp        MLX90614 contactless thermometer
//    vitals_sensor    MAX30105 heart rate and SpO2
//    user_interface   the three operating modes and their buttons
//    whatsapp         notifications through the gateway
//    cloud_sync       Firebase Realtime Database
//    web_portal       local pages, served in safe mode only
//
//  Wiring, cloud layout and mode behaviour are in README.md.
// ============================================================

void setup()
{
    lifeGuardNodeInit();
}

void loop()
{
    lifeGuardNodeUpdate();
}
