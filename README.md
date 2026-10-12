# LifeGuard Node

A monitoring node for a pipeline station, built on an ESP32.

It does three things at once. It **watches the pipeline**: a sensor node out at the pipe reports a situation over ESP-NOW, and this board raises the alarm when the situation is anything but normal. It **watches its own station**: air temperature, humidity and a gas reading from an analog probe. And it **screens the people standing next to it**: a contactless forehead temperature, then a heart rate and blood oxygen reading from a finger.

Everything it measures goes to a Firebase Realtime Database, any alert goes out over WhatsApp as well as to the local relay, buzzer and display — and if the cloud cannot be reached at boot, the node does not stop working: it serves its own web portal instead.

The firmware is a PlatformIO project built from modules, one per concern, with a single environment for the one board it runs on.

| Board | Environment | Framework |
| --- | --- | --- |
| ESP32 DevKit (ESP32-WROOM-32) | `esp32dev` | Arduino |

---

## Table of Contents

- [Features](#features)
- [How It Works](#how-it-works)
- [Operating Modes](#operating-modes)
- [Alerts, and What They Do](#alerts-and-what-they-do)
- [Safe Mode](#safe-mode)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Hardware Requirements](#hardware-requirements)
- [Wiring and Pin Map](#wiring-and-pin-map)
- [Getting Started](#getting-started)
- [Credentials and Secrets](#credentials-and-secrets)
- [Cloud Data Model](#cloud-data-model)
- [Local Web Portal API](#local-web-portal-api)
- [Configuration Reference](#configuration-reference)
- [Troubleshooting](#troubleshooting)
- [Design Notes and Limitations](#design-notes-and-limitations)
- [License](#license)

---

## Features

- **Pipeline link over ESP-NOW** — the peer reports a situation string; the node mirrors it, raises the alert and — this is what makes the link useful in both directions — answers a reset with a `corrected` acknowledgement the peer can see.
- **Station environment monitoring** — DHT11 air temperature and humidity plus a gas probe on the ADC, with a five-second record in the cloud.
- **Two health-screening modes** — a contactless MLX90614 forehead reading, then a MAX30105 heart rate / SpO2 reading, chained in the order a screening actually happens.
- **One alert layer** — the pipeline alarm and the environment alarm feed the same policy, so the relay, the buzzer, the LED, the display banner and the WhatsApp notification can never disagree about what is happening.
- **Notifications that do not repeat themselves** — sent on the rising edge, with a five-minute cooldown per source.
- **Safe mode** — if the cloud does not answer within 15 seconds, the node serves its own portal over the network it is already on, so the readings and the reset button stay reachable.
- **Self-contained pages** — no CDN, no external assets, so the portal works on a phone that joined the same network with no internet behind it.
- **Fail-fast configuration** — the build stops with a clear `#error` if the secrets file is missing.
- **Secrets kept out of Git** — every credential lives in `include/secrets.h`, which is git-ignored, with a committed `include/secrets.example.h` template.
- **One module per concern** — pins and timings in one header, a `.cpp` per module, and a `main.cpp` that does nothing but hand over to the node flow.

---

## How It Works

### Boot

1. Configure the alert outputs, with the relay released and the buzzer silent. That happens before anything else, so an interrupted boot cannot leave the outlet energised.
2. Start the display, then join the Wi-Fi network. The association is the slowest step, so row 1 counts the retries with dots.
3. Sign in to the Firebase Realtime Database and wait up to 15 s, driving the library's async loop while it waits.
4. **Cloud ready:** publish `Online` to the database. **Cloud not ready:** enter [safe mode](#safe-mode) — announce it with the node's IP address, start the web portal and hold that screen for four seconds.
5. Start the ESP-NOW link on the channel the Wi-Fi station landed on, then the sensors and the buttons.
6. Enable alerts, show `SYSTEM READY`.

Alerts stay gated until step 6. A situation message that arrives during boot is mirrored and shown, but does not raise an alarm: the node is not yet in a position where it can be relied on to have handled it.

### One pass of `loop()`

1. Serve whichever network is active — the portal in safe mode, the Firebase client otherwise.
2. Sample the station: the gas channel every pass, the DHT11 no more than once every 2 s (it cannot answer faster than that).
3. Update the alert layer: hysteresis on the environment reading, the pipeline situation as it stands, then the relay, buzzer, LED, banner and any notification the rising edges earned.
4. Publish to the cloud, every 5 s, and check the node's remote reset flag.
5. Hand the display to the selected operating mode — **only while no alert is up**, because the banner owns the screen then.

The two health modes are the only blocking paths in the loop: a vitals reading stops the node for the 10 s acquisition window plus the 8 s the result stays on screen. A pipeline alert that arrives during a vitals reading is received by the ESP-NOW callback (which is not blocked) and is handled as soon as the mode hands control back.

### Timing

```
        every pass            every 2 s            every 5 s
   ┌────────────────┐   ┌──────────────────┐   ┌────────────────┐
   │ gas probe      │   │ DHT11 reading    │   │ Firebase write │
   │ button scan    │   │ (cached between) │   │ remote reset   │
   │ screen refresh │   └──────────────────┘   └────────────────┘
   │ (500 ms)       │
   └────────────────┘
              ▲
              │ ESP-NOW: as soon as the pipeline node reports
```

---

## Operating Modes

| Mode | Selected by | Display | Ends when |
| --- | --- | --- | --- |
| `0` Environment | default, or button 1 | `G:50 H:60% T:24` / `STATUS: GOOD` | button 1 |
| `1` Contactless temperature | button 1 | `SCAN TEMP...` / `LIVE: 36.5C`, then `TEMP CAPTURED` / `HOLD:36.5C NEXT>` | button 2 captures, then button 2 again |
| `2` Vitals | button 2 from a captured screen | `PLACE FINGER ON` / `THE BPM SENSOR`, then `HR:75 SpO2:98%` / `TEMP: 36.5C (OK)` | 8 s after the reading, then back to mode 0 |

Button 1 switches between mode 0 and mode 1. Button 2 is the "capture / next step" action: it captures the live temperature in mode 1, and moves on to mode 2 once that capture is held. A fever capture (40 °C or above) stops the chain and pulses the alarm until button 2 dismisses it.

Both buttons are wired to ground and read `LOW` when pressed.

---

## Alerts, and What They Do

An alert is raised by either source and cleared by neither until it is over:

| Source | Raised when | Cleared when |
| --- | --- | --- |
| Pipeline | the peer reports anything other than `Normal` | the peer reports `Normal`, or the alert is cleared by hand |
| Environment | gas above 50 %, air temperature above 40 °C, humidity above 90 % or below 10 % | gas below 45 % **and** air temperature below 38 °C |

The two windows are deliberately not each other's negation, and humidity can only raise an alert, never clear one — both are carried over from the original node, and they are what stop a board sitting at a threshold from flapping.

While an alert is up:

- the **relay** is energised, cutting power to the station outlet;
- the **buzzer** and the **status LED** pulse at 600 ms intervals;
- the **display** shows `!! ALERT !!` and names the source, taking the screen from whatever mode was showing;
- a **WhatsApp message** goes out once per rising edge — `PIPELINE ALERT: <situation>` or `ENV ALERT: Gas: <n>% Temp: <n>C` — with a five-minute cooldown per source;
- the modes are paused until the alert clears.

Clearing an alert — from the portal, from the `remoteReset` flag in the database, or by the peer returning to `Normal` — releases the relay, silences the buzzer, and (when it was the pipeline's alert) sends the peer a `corrected` acknowledgement. A manual clear also shows `ALERT CLEARED` for a second and a half.

---

## Safe Mode

"Safe mode" here means **the node has no cloud**, not that the board is in the chip's bootloader mode.

If the Firebase sign-in does not complete within 15 s, the node:

- announces `SAFE MODE: ON` and its IP address on the display and holds it for four seconds;
- starts the local web portal on port 80, on whatever network the station joined;
- keeps sampling, keeps raising alerts from any source, and keeps mirroring the pipeline situation;
- stops writing to the cloud and stops sending WhatsApp messages, since both need the same link.

It does **not** retry the sign-in: the node stays local until it is restarted. That is deliberate — a node that alternates between cloud and local every few minutes is harder to reason about than one that picked an answer at boot.

---

## Architecture

```
      pipeline node (peer)
              │  ESP-NOW, one PipeMessage
              ▼
 ┌──────────────────────────────────────────────────────────────┐
 │                       LifeGuard Node                         │
 │                                                              │
 │  pipe_link ─────┐                                            │
 │                 ├──▶ alert_manager ──▶ alarm (relay/buzzer)   │
 │  env_monitor ───┘         │           whatsapp               │
 │                           └─────────▶ lcd_display (banner)   │
 │                                                              │
 │  body_temp ─────┐                                            │
 │                 ├──▶ user_interface ──▶ lcd_display (screens) │
 │  vitals_sensor ─┘         │                                  │
 │                           └─────────▶ cloud_sync             │
 │                                                              │
 │  cloud_sync ◀──▶ Firebase Realtime Database                  │
 │  web_portal ◀──▶ phone on the same network (safe mode only)   │
 └──────────────────────────────────────────────────────────────┘
```

`life_guard_node` owns the order of the arrows; `main.cpp` owns nothing at all.

Two shared services sit on the outside of that diagram: `lcd_display` (every screen goes through one padding rule, so a row is always 16 characters and nothing from the previous screen shows through) and `alarm` (every output that indicates an alert).

---

## Project Structure

```
life-guard-node/
├── platformio.ini              One environment: esp32dev
├── README.md
├── LICENSE
├── .gitignore                  Build output, editor state, include/secrets.h
├── include/
│   ├── config.h                Pins, timings and thresholds
│   ├── secrets.example.h       Credentials template (committed)
│   ├── secrets.h               Credentials (git-ignored, yours to create)
│   ├── life_guard_node.h       Node flow: boot and one pass of loop()
│   ├── alert_manager.h          Pipeline + environment alert policy
│   ├── alarm.h                  Relay, buzzer and status LED outputs
│   ├── cloud_sync.h             Firebase Realtime Database
│   ├── web_portal.h             Local pages, safe mode only
│   ├── whatsapp.h               Notifications through the gateway
│   ├── pipe_link.h              ESP-NOW link and its wire contract
│   ├── env_monitor.h            DHT11 + gas probe
│   ├── body_temp.h              Contactless thermometer
│   ├── vitals_sensor.h          Heart rate and SpO2
│   ├── user_interface.h         Operating modes and buttons
│   └── lcd_display.h            I2C display rendering
├── src/                        One .cpp per header, plus main.cpp
└── legacy/                     The original prototype sketch (not compiled)
```

`legacy/life_guard_node_v5.ino` is the single sketch this project was ported from: one file, everything in it, credentials included. It is kept for reference. PlatformIO compiles `src/` only, so it sits there without affecting the build — and the credentials in it have been replaced with placeholders, because a working Wi-Fi password has no business in a file that gets committed.

---

## Hardware Requirements

| Part | Quantity | Notes |
| --- | --- | --- |
| ESP32 DevKit (ESP32-WROOM-32) | 1 | The node itself |
| 16×2 I2C LCD (PCF8574 backpack) | 1 | Address `0x27` |
| DHT11 temperature/humidity sensor | 1 | DHT22 also works; change `DHT_MODEL` |
| Analog gas probe (MQ-style) | 1 | Analog output on the ADC |
| MLX90614 contactless thermometer | 1 | I2C, address `0x5A` |
| MAX30105 pulse oximeter | 1 | I2C, address `0x57` |
| 1-channel relay module | 1 | Opto-isolated modules are worth the extra cost |
| Active buzzer | 1 | Driven by `tone()` |
| LED + 220 Ω resistor | 1 | Status indication |
| Momentary push buttons | 2 | Wired to ground, no external pull-ups needed |
| 10 kΩ resistor | 1 | Pull-up on the DHT11 data line |
| 5 V supply, ≥ 1 A | 1 | Not a weak USB port: the relay coil and the LCD backlight both draw from it |
| Jumper wires | — | Two for I2C, plus one per sensor, button and output |

All three I2C devices share pins 21 and 22 and have different addresses, so no multiplexer is needed.

---

## Wiring and Pin Map

| Signal | GPIO | Direction | Notes |
| --- | --- | --- | --- |
| LCD SDA / SCL | 21 / 22 | out | I2C bus, 100 kHz |
| DHT11 data | 13 | in/out | Needs the 10 kΩ pull-up to 3.3 V |
| Gas probe analog out | 36 | in | ADC1, and an input-only pin |
| Relay input | 23 | out | HIGH energises the relay |
| Buzzer | 25 | out | Driven with `tone()` |
| Status LED | 26 | out | Through a 220 Ω resistor to ground |
| Button 1 (mode) | 32 | in | To ground; internal pull-up |
| Button 2 (capture / next) | 33 | in | To ground; internal pull-up |
| (I2C) LCD / MLX90614 / MAX30105 | — | — | Addresses `0x27` / `0x5A` / `0x57` |

GPIO36 is input-only and sits on ADC1, which means it keeps working while the Wi-Fi radio is active. Avoid ADC2 pins (0, 2, 4, 12–15, 25–27) for analog inputs on a board that uses Wi-Fi at all.

GPIO32 and GPIO33 are the two pins the original node used for its buttons; they are not strapping pins and not shared with any of the peripherals above.

### Pipeline peer

| Setting | Value | Where |
| --- | --- | --- |
| Peer MAC address | `D4:E9:F4:64:D5:58` | `PIPE_PEER_MAC` in [config.h](./include/config.h) |
| Channel | the one the Wi-Fi station joined | taken at boot in [pipe_link.cpp](./src/pipe_link.cpp) |
| Encryption | none | ESP-NOW default |

---

## Getting Started

### 1. Prerequisites

- [VS Code](https://code.visualstudio.com/) with the [PlatformIO IDE extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide), or PlatformIO Core 6.x on the CLI
- `platform = espressif32 @ ^7.0.1`, `framework = arduino` (pinned in `platformio.ini`, because the Arduino core changes between platform majors)

Dependencies are declared in `platformio.ini` and installed automatically on the first build:

| Library | Used for |
| --- | --- |
| `mobizt/FirebaseClient` | Realtime Database over TLS (pulls in `ESP_SSLClient`) |
| `marcoschwartz/LiquidCrystal_I2C` | The display |
| `adafruit/DHT sensor library` | DHT11 (pulls in Adafruit Unified Sensor) |
| `adafruit/Adafruit MLX90614 Library` | Contactless thermometer |
| `sparkfun/SparkFun MAX3010x …` | MAX30105, with Maxim's SpO2 algorithm |

### 2. Create your credentials file

Nothing compiles until this exists — the build deliberately stops with:

```
#error "Missing include/secrets.h - copy include/secrets.example.h to include/secrets.h ..."
```

```bash
copy include\secrets.example.h include\secrets.h     # Windows
cp include/secrets.example.h include/secrets.h       # macOS / Linux
```

Then edit `include/secrets.h` — see [Credentials and Secrets](#credentials-and-secrets).

### 3. Build and upload

Pick the environment in the PlatformIO toolbar and press **Upload**, or use the CLI:

```bash
pio run -e esp32dev -t upload
```

Or build without uploading:

```bash
pio run -e esp32dev
```

### 4. Run

Open the serial monitor at 115200 baud:

```
[LifeGuardNode] starting up
[wifi] connected, ip 192.168.1.42
[cloud] signed in as node@example.com
[pipe] listening on channel 6
[LifeGuardNode] ready
```

An alert, as it happens:

```
[pipe] alert: Leak detected
[alert] pipeline: Leak detected
[whatsapp] notification sent
```

### 5. Open the portal

In normal operation the node has nothing listening — the cloud is the interface. The portal is started in [safe mode](#safe-mode) only, and the display shows the address to browse to at boot. On a phone joined to the same network, that address serves the **Local Access Portal**; `/admin` has the pipeline status and the reset button, `/public` shows the latest vitals and the environment.

---

## Credentials and Secrets

Every credential lives in `include/secrets.h`, which **is listed in [.gitignore](./.gitignore) and must never be committed**. The file is seeded with the values that used to be hard-coded in the original sketch, so the node keeps talking to the same network, database and gateway it always did — but they now stay on the machine that builds the firmware.

| Key | Used for | Where to get it |
| --- | --- | --- |
| `WIFI_SSID` / `WIFI_PASSWORD` | the network the node joins (2.4 GHz) | your router |
| `FIREBASE_API_KEY` | Firebase project web API key | Firebase console → Project settings |
| `FIREBASE_USER_EMAIL` / `_PASSWORD` | the node's own database user | Firebase console → Authentication |
| `FIREBASE_DATABASE_URL` | the Realtime Database instance | Firebase console → Realtime Database |
| `WHATSAPP_PHONE` | the number that receives alerts | the recipient's phone |
| `WHATSAPP_API_KEY` | gateway account key | your WhatsApp gateway provider |
| `WHATSAPP_BASE_URL` | gateway endpoint | your WhatsApp gateway provider |

> **Rotate all of these.** They were published in plain source in the original sketch, so anyone who has read it has seen the Wi-Fi password, the Firebase user's password and the gateway key. That applies whether or not the repository was ever pushed.

> **Lock the Firebase user down.** The node signs in as a user and writes a fixed set of paths; the database rules should restrict that account to those paths and nothing else, so a leaked `secrets.h` cannot be used to rewrite the whole project. The web API key on its own is not a secret — the account behind it is.

Before pushing the project to GitHub:

```bash
git check-ignore -v include/secrets.h    # must print the .gitignore match
git status --porcelain                   # secrets.h must not appear
```

---

## Cloud Data Model

The node writes these paths in the Realtime Database, and reads one back:

| Path | Type | Written | Meaning |
| --- | --- | --- | --- |
| `/pipeline/status` | string | once, at boot | `Online` while the node is up |
| `/pipeline/situation` | string | every 5 s | the last situation the peer reported |
| `/environment/gas` | number | every 5 s | gas reading, whole percent |
| `/environment/temp` | number | every 5 s | air temperature in °C |
| `/environment/hum` | number | every 5 s | relative humidity in % |
| `/health/last_bpm` | number | after a vitals reading | heart rate in bpm |
| `/health/last_spo2` | number | after a vitals reading | blood oxygen in % |
| `/pipeline/remoteReset` | boolean | read every 5 s | writing `true` clears the alerts on the node |

A reading the DHT11 did not answer with is not written at all, so the database keeps the last good value instead of losing it to a `NaN`. The node clears `/pipeline/remoteReset` itself once it has acted on it.

---

## Local Web Portal API

Served on port 80, in safe mode only.

| Method | Path | Returns |
| --- | --- | --- |
| `GET` | `/` | The portal landing page (`text/html`) |
| `GET` | `/admin` | Pipeline status, environment panel, reset button |
| `GET` | `/public` | Latest vitals, environment, recommendation |
| `GET` | `/api` | The JSON the pages poll (`application/json`) |
| `POST` | `/reset` | Clears the alerts, then redirects to `/admin` |

`/api`:

```json
{"sit":"Leak detected","gas":62,"temp":26.5,"hum":60.2}
```

| Field | Type | Meaning |
| --- | --- | --- |
| `sit` | string | The last situation the pipeline peer reported |
| `gas` | number | Gas reading, whole percent |
| `temp` | number or `null` | Air temperature in °C |
| `hum` | number or `null` | Relative humidity in % |

`null` means the DHT11 did not answer, and the pages render it as `--`. Every endpoint is a read-only `GET` apart from `/reset`.

---

## Configuration Reference

Everything tunable lives in [include/config.h](./include/config.h).

### I2C and display

| Constant | Default | Meaning |
| --- | --- | --- |
| `PIN_I2C_SDA` / `PIN_I2C_SCL` | 21 / 22 | The ESP32 defaults, made explicit |
| `I2C_CLOCK_HZ` | `100000` | Shared by all three I2C devices |
| `LCD_I2C_ADDRESS` | `0x27` | Common backpack address; `0x3F` is the other |
| `LCD_COLUMNS` / `LCD_ROWS` | 16 / 2 | Also the width a row is padded and clipped to |

### Alert outputs and buttons

| Constant | Default | Meaning |
| --- | --- | --- |
| `PIN_RELAY` | 23 | HIGH energises the relay |
| `PIN_BUZZER` / `PIN_STATUS_LED` | 25 / 26 | The audible and visible halves of the pulse |
| `ALERT_TONE_HZ` | `1000` | Alert tone |
| `ALERT_BEEP_INTERVAL_MS` | `600` | Half-period of the pulse |
| `UI_TONE_HZ` / `UI_TONE_MS` | `2000` / `50` | Button confirmation beep |
| `LCD_REFRESH_MS` | `500` | How often a screen is redrawn |
| `PIN_BUTTON_MODE` / `PIN_BUTTON_VITALS` | 32 / 33 | Wired to ground |

### Station environment

| Constant | Default | Meaning |
| --- | --- | --- |
| `PIN_DHT` / `DHT_MODEL` | 13 / `DHT11` | Air sensor |
| `DHT_REFRESH_MS` | `2000` | How often the DHT11 is actually read |
| `PIN_GAS` | 36 | ADC1, input-only |
| `GAS_ALERT_PCT` / `GAS_CLEAR_PCT` | 50 / 45 | Raise and clear thresholds |
| `AIR_TEMP_ALERT_C` / `AIR_TEMP_CLEAR_C` | 40.0 / 38.0 | Raise and clear thresholds |
| `HUMIDITY_ALERT_HIGH_PCT` / `_LOW_PCT` | 90.0 / 10.0 | Raise only |

### Health sensors

| Constant | Default | Meaning |
| --- | --- | --- |
| `BODY_TEMP_MIN_C` | 10.0 | Below this, nothing is in front of the sensor |
| `BODY_FEVER_C` | 40.0 | Fever threshold for the alarm |
| `BODY_TEMP_DECIMALS` | 1 | Decimal places on screen |
| `VITALS_BUFFER_SIZE` | 100 | Samples the algorithm analyses |
| `VITALS_ACQUIRE_WINDOW_MS` | `10000` | How long the node samples for |
| `VITALS_HOLD_MS` | `8000` | How long a result stays on screen |
| `VITALS_MIN_VALID_BPM` | 40 | Below this a reading is rejected |
| `VITALS_DEFAULT_BPM` / `_SPO2_PCT` | 75 / 98 | Fallback when a reading is rejected |

### Link and cloud

| Constant | Default | Meaning |
| --- | --- | --- |
| `PIPE_SITUATION_LEN` | 16 | Bytes of the situation field — a wire contract |
| `PIPE_PEER_MAC` | `D4:E9:F4:64:D5:58` | The pipeline node's address |
| `CLOUD_READY_TIMEOUT_MS` | `15000` | How long boot waits for the cloud before safe mode |
| `CLOUD_SYNC_INTERVAL_MS` | `5000` | Publish interval |
| `CLOUD_TEMP_DECIMALS` | 2 | Decimal places published for temp/humidity |
| `WHATSAPP_COOLDOWN_MS` | `300000` | Five minutes, per category |
| `WHATSAPP_HTTP_TIMEOUT_MS` | `4000` | Gateway request timeout |
| `HTTP_PORT` | 80 | Portal port |
| `WIFI_RETRY_INTERVAL_MS` | `500` | Gap between association retries |
| `SAFE_MODE_HOLD_MS` / `RESET_MESSAGE_HOLD_MS` | 4000 / 1500 | How long two messages stay on screen |

---

## Troubleshooting

| Symptom | Likely cause / fix |
| --- | --- |
| `Missing include/secrets.h` during build | Copy `include/secrets.example.h` to `include/secrets.h`. This is intentional |
| Boot never leaves `CONNECTING WIFI` | Wrong SSID/password in `secrets.h`, or a 5 GHz-only network — the ESP32 is 2.4 GHz only |
| Boot ends at `SAFE MODE: ON` | The Firebase sign-in did not complete in 15 s: wrong API key, wrong user, wrong database URL, or no internet. The node is local-only until it is restarted |
| `[cloud] signed in` but nothing appears in the database | The database rules are refusing the writes: check the rules against the paths in [Cloud Data Model](#cloud-data-model) |
| `[pipe] listening on channel 6` but no situations ever arrive | The peer is on a different channel (it follows the Wi-Fi station's channel at boot), a different MAC is configured, or the peer is not transmitting. Both ends must agree on the channel |
| `[pipe] dropped N-byte message` | Something else is transmitting to this node, or the peer's struct layout has drifted — `PipeMessage` must match byte for byte |
| The peer never sees the acknowledgement | `esp_now_send` only reaches a registered peer; check the MAC, and that the peer registered this node on its side too |
| Buzzer keeps sounding after the situation cleared | Check `alertManagerClear()` was reached — a peer that never reports `Normal` needs the reset button or the database flag |
| `ENV ALERT` at night with a normal reading | Humidity above 90 % raises the alert on its own; that threshold is deliberate |
| Environment screen shows `--` | The DHT11 did not answer. Normal now and then; if it never answers, check the 10 kΩ pull-up, the 3.3 V supply and `DHT_MODEL` |
| Contactless reading is always room temperature | Nothing is in the sensor's field of view, so a capture is refused below `BODY_TEMP_MIN_C` |
| Vitals always report `75` / `98` | Those are the fallbacks: the algorithm rejected the reading. Signal comes from pressure, not light — a fingertip covering the sensor window with no gap usually fixes it |
| LCD is blank, or shows blocks | I2C address (`0x27` vs `0x3F`), SDA/SCL swapped, or the contrast potentiometer on the backpack |
| Node browns out, or the relay chatters | The relay coil plus the LCD backlight want more than a weak USB port supplies; use a 5 V ≥ 1 A supply |
| Portal unreachable although safe mode is announced | Browse to the exact address on the display, and make sure the phone is on the same network — the node serves on the network it joined, it does not create one |

---

## Design Notes and Limitations

- **This is not a medical device.** The contactless reading assumes a forehead filling the sensor's field of view at the right distance, and the pulse oximeter is a fitness-grade sensor. Neither has been calibrated or validated against a clinical reference, and none of these readings should be used for diagnosis or treatment.
- **The contactless alarm and the public recommendation disagree, on purpose.** The device pulses `HIGH FEVER ALERT` at 40 °C while the public page calls anything above 38 °C a fever, because those are the thresholds the original node used. They are two constants in two files if you would rather they matched.
- **A rejected vitals reading is indistinguishable from a real one.** The fallbacks are 75 bpm and 98 %, so a failed measurement looks like a healthy patient. Nothing in the cloud record says which it was — worth a flag in the payload if this is ever used for anything that matters.
- **The pipeline link is unauthenticated and its payload is not validated.** ESP-NOW runs unencrypted, the peer is trusted by MAC address, and the situation string is whatever the peer sends. Anyone with the peer's address and a radio on the same channel can raise an alert here. This is a local sensor link, not a control channel — do not hang anything off it that a forged packet could drive.
- **The link follows the Wi-Fi channel.** ESP-NOW is pinned to the channel the station associated with, and nothing re-pins it if the station roams or the access point moves channels. In that case the link goes quiet until the node is restarted — the dashboard's `situation` stops updating, which is the visible symptom.
- **The TLS certificate chain is not verified.** `setInsecure()` is deliberate: the ESP32 has no clock it can trust before its first sync, so a pinned root that is checked against a 1970 date fails exactly when it is needed. The traffic is encrypted, but an active attacker on the path could impersonate the database. The digest-based alternative is `setInsecure()` plus a `dateTime()` call after the first sync.
- **The WhatsApp gateway is plain HTTP.** The request carries the gateway key in its query string over an unencrypted connection, so anyone on the path can read it. Treat the key as low-value, rotate it if it has ever travelled over a network you do not control, and move the endpoint to TLS if your provider offers one.
- **Notifications are best-effort.** A message is sent once per rising edge with a five-minute cooldown per source, and a failed send is retried on the next edge, not by a queue. An alert raised while the network is down is never notified unless it clears and returns.
- **Alerts pause the modes, and modes block the loop.** While an alert is up the display belongs to the banner, so a temperature or vitals reading in progress is interrupted. A vitals reading blocks the loop for up to 18 s; anything that arrives during it is handled after the mode hands control back.
- **The buzzer is silenced when the last alert clears.** The original node left it sounding when the pipeline path cleared — this port silences it on the falling edge, whichever source let go.
- **A row on the environment screen is clipped at 16 characters.** The original let it wrap onto the status row and overwrite it; clipping keeps the status line intact.
- **A missing DHT11 reading reads as "no evidence of a problem".** Every comparison against `NaN` is false, so a silent sensor cannot raise the environment alert — it shows as `--` and `null` instead. If silence itself should be an alert, that is a check on the reading's age rather than on its value.
- **Humidity can raise an alert but never clear one**, and the clear window does not consider humidity at all. Carried over from the original.
- **`safeMode` is decided once, at boot.** The node does not fall back to the portal if the cloud link dies later, and it does not try the cloud again if it starts local. Restarting is the switch.
- **The portal has no login.** Anything that reaches port 80 in safe mode can read the readings and press RESET. It is reachable from the network the node joined, so treat that network as trusted.
- **Nothing is persisted locally.** The database holds the latest value per path, and the node holds the last situation in RAM; there is no history, no ring buffer and no SD card. A gap in the record is a gap in the evidence.
- **The Firebase user's password is stored in plaintext on the device.** That is how the email/password flow works; the database rules are what limit the damage if the board is taken.

---

## License

Released under the MIT License — see [LICENSE](./LICENSE).
