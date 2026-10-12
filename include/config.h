#pragma once

#include <Arduino.h>

// ============================================================
//  PROJECT: LifeGuard Node
//  Single source of truth for pins, timings and thresholds.
//
//  Every module includes this header instead of hard-coding a pin
//  or a timing constant, so the wiring is described in exactly one
//  place. Credentials are deliberately NOT here: they live in
//  include/secrets.h, which is git-ignored.
// ============================================================

// --- Serial ---
#define SERIAL_BAUD 115200

// --- Credentials ---
// Real values live in include/secrets.h, which is git-ignored.
// Copy include/secrets.example.h to include/secrets.h first.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing include/secrets.h - copy include/secrets.example.h to include/secrets.h and fill in your credentials."
#endif

// ============================================================
//  I2C BUS AND DISPLAY
//  The LCD, the contactless thermometer and the pulse oximeter
//  share one bus, which is why every driver is started with the
//  same clock.
// ============================================================
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22
#define I2C_CLOCK_HZ 100000

#define LCD_I2C_ADDRESS 0x27 // 0x3F is the other common backpack address
#define LCD_COLUMNS 16
#define LCD_ROWS 2

// ============================================================
//  ALERT OUTPUTS
//  The relay cuts the station's outlet while an alert is up; the
//  buzzer and the status LED pulse with it.
// ============================================================
#define PIN_RELAY 23
#define PIN_BUZZER 25
#define PIN_STATUS_LED 26

#define ALERT_TONE_HZ 1000
#define ALERT_BEEP_INTERVAL_MS 600 // On/off half-period of the alert pulse
#define UI_TONE_HZ 2000            // Short confirmation beep for a button
#define UI_TONE_MS 50
#define LCD_REFRESH_MS 500         // How often a screen is redrawn

// ============================================================
//  BUTTONS
//  Wired to ground, so they read LOW when pressed.
// ============================================================
#define PIN_BUTTON_MODE 32   // Environment <-> contactless temperature
#define PIN_BUTTON_VITALS 33 // Capture, then next step

// ============================================================
//  OPERATING MODES
// ============================================================
#define MODE_ENVIRONMENT 0
#define MODE_BODY_TEMP 1
#define MODE_VITALS 2

#define MODE_SWITCH_GUARD_MS 300 // Settling time after a button beep

// ============================================================
//  STATION ENVIRONMENT  (DHT11 + analog gas probe)
// ============================================================
#define PIN_DHT 13
#define DHT_MODEL DHT11

// A DHT11 cannot answer faster than about once a second, so the
// module caches a reading and refreshes it at this interval.
#define DHT_REFRESH_MS 2000

// The gas probe is a 12-bit ADC channel: 0..4095 scaled to percent.
// GPIO36 sits on ADC1 and is input-only, which is all an ADC pin
// needs — and ADC1 keeps working while the Wi-Fi radio is active.
#define PIN_GAS 36
#define GAS_ADC_MIN 0
#define GAS_ADC_MAX 4095
#define GAS_SCALE_MAX_PCT 100

// Alert window, carried over from the original node: the alert is
// raised by any reading outside the wide window and only cleared
// once gas and temperature are back inside the narrow one.
#define GAS_ALERT_PCT 50
#define GAS_CLEAR_PCT 45
#define AIR_TEMP_ALERT_C 40.0f
#define AIR_TEMP_CLEAR_C 38.0f
#define HUMIDITY_ALERT_HIGH_PCT 90.0f
#define HUMIDITY_ALERT_LOW_PCT 10.0f

// ============================================================
//  CONTACTLESS THERMOMETER  (MLX90614 on the I2C bus)
// ============================================================
// A live reading below this is treated as "no forehead in front of
// the sensor", so it cannot be captured.
#define BODY_TEMP_MIN_C 10.0f
#define BODY_FEVER_C 40.0f
#define BODY_TEMP_DECIMALS 1

// ============================================================
//  VITALS  (MAX30105 pulse oximeter)
// ============================================================
#define VITALS_BUFFER_SIZE 100
#define VITALS_ACQUIRE_WINDOW_MS 10000
#define VITALS_MIN_VALID_BPM 40
#define VITALS_DEFAULT_BPM 75     // Shown when the algorithm rejects a reading
#define VITALS_DEFAULT_SPO2_PCT 98
#define VITALS_HOLD_MS 8000       // How long the result stays on screen

// Arguments of particleSensor.setup(), named after the driver's own
// documentation: brightness, sample average, LED mode, sample rate,
// pulse width, ADC range.
#define VITALS_LED_BRIGHTNESS 30
#define VITALS_SAMPLE_AVERAGE 4
#define VITALS_LED_MODE 2
#define VITALS_SAMPLE_RATE 100
#define VITALS_PULSE_WIDTH 411
#define VITALS_ADC_RANGE 4096

// ============================================================
//  PIPELINE LINK  (ESP-NOW, point to point)
// ============================================================
// The payload layout must match the pipeline node's copy of
// PipeMessage byte for byte — see include/pipe_link.h.
#define PIPE_SITUATION_LEN 16

// MAC of the pipeline node this board talks to. ESP-NOW is not
// encrypted here, so the peer is identified by its address rather
// than by any key material.
#define PIPE_PEER_MAC { 0xD4, 0xE9, 0xF4, 0x64, 0xD5, 0x58 }

// ============================================================
//  CLOUD  (Firebase Realtime Database)
// ============================================================
// If the client is not signed in after this long, the node gives up
// on the cloud and runs the local portal instead (safe mode).
#define CLOUD_READY_TIMEOUT_MS 15000
#define CLOUD_SYNC_INTERVAL_MS 5000
#define CLOUD_SYNC_SLICE_MS 100      // Poll slice while waiting for sign-in
#define CLOUD_TEMP_DECIMALS 2        // Decimal places published for temp/humidity

#define CLOUD_PATH_STATUS "/pipeline/status"
#define CLOUD_PATH_REMOTE_RESET "/pipeline/remoteReset"
#define CLOUD_PATH_SITUATION "/pipeline/situation"
#define CLOUD_PATH_GAS "/environment/gas"
#define CLOUD_PATH_TEMP "/environment/temp"
#define CLOUD_PATH_HUMIDITY "/environment/hum"
#define CLOUD_PATH_LAST_BPM "/health/last_bpm"
#define CLOUD_PATH_LAST_SPO2 "/health/last_spo2"
#define CLOUD_STATUS_ONLINE "Online"

// ============================================================
//  WHATSAPP NOTIFICATIONS
// ============================================================
// One cooldown per category, so a fault that stays active does not
// become one message per loop pass.
#define WHATSAPP_COOLDOWN_MS 300000UL
#define WHATSAPP_HTTP_TIMEOUT_MS 4000

// ============================================================
//  LOCAL WEB PORTAL  (safe mode only)
// ============================================================
#define HTTP_PORT 80

// ============================================================
//  BOOT TIMINGS
// ============================================================
#define WIFI_RETRY_INTERVAL_MS 500
#define SAFE_MODE_HOLD_MS 4000     // Hold the safe-mode screen so it can be read
#define RESET_MESSAGE_HOLD_MS 1500 // Hold "ALERT CLEARED"
