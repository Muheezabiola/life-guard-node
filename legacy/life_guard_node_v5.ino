/**
 * LifeGuard Node — original prototype sketch
 *
 * This is the sketch the PlatformIO project in src/ was ported from.
 * It is kept for reference and is not compiled: PlatformIO only builds
 * src/, so nothing here affects the firmware.
 *
 * The credentials below used to be the real ones. They are now
 * placeholders — the live values live in include/secrets.h, which is
 * git-ignored — because a working Wi-Fi password, Firebase user
 * password and gateway key have no business sitting in a source file
 * that gets committed.
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <Adafruit_MLX90614.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <HTTPClient.h>
#include <WebServer.h>

#define ENABLE_USER_AUTH
#define ENABLE_DATABASE
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>

#include "MAX30105.h"
#include "spo2_algorithm.h"
#include "heartRate.h"

// ================= CONFIGURATION =================
// Placeholders: the real values live in include/secrets.h.
#define WIFI_SSID "your-network"
#define WIFI_PASS "your-password"
#define API_KEY "your-web-api-key"
#define USER_EMAIL "node@example.com"
#define USER_PASSWORD "your-firebase-user-password"
#define DATABASE_URL "https://your-project-default-rtdb.firebaseio.com"

const String WHATSAPP_NUMBER = "2348000000000";
const String WHATSAPP_APIKEY = "your-gateway-api-key";
const String WHATSAPP_BASE_URL = "http://api.whatabot.net/whatsapp/sendMessage?";

uint8_t pipelineAddress[] = { 0xD4, 0xE9, 0xF4, 0x64, 0xD5, 0x58 };

typedef struct struct_message {
  char situation[16];
  bool corrected;
} struct_message;

struct_message incomingPipeData;
struct_message outgoingPipeData;
esp_now_peer_info_t peerInfo;

#define DHTPIN 13
#define DHTTYPE DHT11
const int gasPin = 36;
const int outletRelayPin = 23;
const int button1Pin = 32;
const int button2Pin = 33;
const int buzzerPin = 25;
const int ledPin = 26;

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
MAX30105 particleSensor;
WebServer server(80);

WiFiClientSecure ssl;
AsyncClientClass aClient(ssl);
FirebaseApp app;
RealtimeDatabase Database;
AsyncResult authResult;
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD);

#define BUFFER_SIZE 100
uint32_t irBuffer[BUFFER_SIZE], redBuffer[BUFFER_SIZE];
int currentMode = 0;
float capturedTemp = 0.0;
int lastHR = 75, lastSPO2 = 98;
bool tempCaptured = false, feverScreenDrawn = false;
unsigned long lastBeepTime = 0, lastLCDUpdate = 0, lastFirebaseUpdate = 0;

unsigned long lastPipeWhatsApp = 0, lastEnvWhatsApp = 0, lastBootWhatsApp = 0;
const unsigned long whatsappCooldown = 300000;

bool buzzerActive = false, pipeAlertActive = false, envAlertActive = false;
bool triggerPipeWhatsapp = false, triggerEnvWhatsapp = false;
bool systemInitialized = false, safeMode = false;

// ================= LOCAL WEB PAGES =================

void handleRoot() {
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, 1'><style>";
  html += "body{font-family:sans-serif; background:#f4f7f6; text-align:center; padding:50px 20px;}";
  html += ".card{max-width:450px; margin:auto; background:white; padding:30px; border-radius:20px; box-shadow:0 10px 25px rgba(0,0,0,0.1);}";
  html += ".btn{display:block; padding:15px; margin:10px 0; background:#3498db; color:white; text-decoration:none; border-radius:10px; font-weight:bold; transition:0.3s;}";
  html += ".btn:hover{opacity:0.8;}</style></head><body><div class='card'><h1>LifeGuard</h1><p>Local Access Portal</p>";
  html += "<a href='/admin' class='btn' style='background:#2c3e50;'>Admin Dashboard</a>";
  html += "<a href='/public' class='btn'>Public Monitor</a></div></body></html>";
  server.send(200, "text/html", html);
}

void handleAdmin() {
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, 1'><style>";
  html += "body{font-family:sans-serif; background:#2c3e50; color:white; padding:20px; text-align:center;}";
  html += ".card{background:#34495e; padding:20px; border-radius:15px; margin:10px auto; max-width:500px;}";
  html += ".btn{background:#e67e22; color:white; padding:20px; border:none; border-radius:10px; width:100%; font-size:1.2em; cursor:pointer;}";
  html += "span{font-weight:bold; color:#f1c40f;} .alert{color:#ff4d4d; animate:blink 1s infinite;}</style>";
  html += "<script>setInterval(()=>{fetch('/api').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('sit').innerText=d.sit; document.getElementById('env').innerText='Gas: '+d.gas+'% | Temp: '+d.temp+'C | Hum: '+d.hum+'%';";
  html += "if(d.sit!='Normal') document.getElementById('sit').className='alert'; else document.getElementById('sit').className='';";
  html += "})}, 2000);</script></head><body><h1>Admin Dashboard</h1>";
  html += "<div class='card'><h2>Pipeline Control</h2><p>Status: <span id='sit'>...</span></p>";
  html += "<form action='/reset' method='POST'><button class='btn'>RESOLVE SITUATION</button></form></div>";
  html += "<div class='card'><h2>Station Environment</h2><p id='env'>Loading sensors...</p></div>";
  html += "<a href='/' style='color:#bdc3c7;'>&larr; Back to Portal</a></body></html>";
  server.send(200, "text/html", html);
}

void handlePublic() {
  String rec = (capturedTemp > 38.0) ? "Fever Detected: Hydrate." : (lastSPO2 < 94) ? "Low Oxygen: Seek air."
                                                                                    : "Vitals Normal.";
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, 1'><style>";
  html += "body{font-family:sans-serif; background:#ecf0f1; padding:20px; text-align:center;}";
  html += ".box{background:white; padding:20px; border-radius:15px; margin:10px auto; max-width:500px; border-top:5px solid #27ae60;}";
  html += ".val{font-size:2em; color:#2c3e50; font-weight:bold;} .rec{background:#d5f5e3; padding:15px; border-radius:5px;}</style>";
  html += "<script>setInterval(()=>{fetch('/api').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('p_env').innerText='Gas: '+d.gas+'% | Temp: '+d.temp+'C | Hum: '+d.hum+'%';";
  html += "})}, 2000);</script></head><body><h1>Public Monitor</h1>";
  html += "<div class='box'><h3>Latest Vitals</h3><p>HR: <span class='val'>" + String(lastHR) + "</span> | SpO2: <span class='val'>" + String(lastSPO2) + "</span>%</p>";
  html += "<p>Body Temp: <span class='val'>" + String(capturedTemp, 1) + "</span>C</p></div>";
  html += "<div class='box'><h3>Environment</h3><p id='p_env'>Loading...</p></div>";
  html += "<div class='box'><h3>Recommendation</h3><p class='rec'>" + rec + "</p></div>";
  html += "<a href='/' style='color:#7f8c8d;'>&larr; Back to Portal</a></body></html>";
  server.send(200, "text/html", html);
}

void handleAPI() {
  String json = "{";
  json += "\"sit\":\"" + String(incomingPipeData.situation) + "\",";
  json += "\"gas\":" + String(map(analogRead(gasPin), 0, 4095, 0, 100)) + ",";
  json += "\"temp\":" + String(dht.readTemperature()) + ",";
  json += "\"hum\":" + String(dht.readHumidity());
  json += "}";
  server.send(200, "application/json", json);
}

// ================= UTILITIES =================

String urlencode(String str) {
  String encoded = "";
  for (int i = 0; i < (int)str.length(); i++) {
    char c = str.charAt(i);
    if (isalnum(c)) encoded += c;
    else if (c == ' ') encoded += "+";
    else {
      encoded += '%';
      if (c < 16) encoded += '0';
      encoded += String(c, HEX);
    }
  }
  return encoded;
}

void sendWhatsappCategorized(String message, int category) {
  if (safeMode || WiFi.status() != WL_CONNECTED) return;
  unsigned long currentTime = millis();
  if (category == 0 && lastBootWhatsApp != 0 && (currentTime - lastBootWhatsApp < whatsappCooldown)) return;
  if (category == 1 && lastPipeWhatsApp != 0 && (currentTime - lastPipeWhatsApp < whatsappCooldown)) return;
  if (category == 2 && lastEnvWhatsApp != 0 && (currentTime - lastEnvWhatsApp < whatsappCooldown)) return;

  noTone(buzzerPin);
  HTTPClient http;
  WiFiClient client;
  String url = WHATSAPP_BASE_URL + "apikey=" + WHATSAPP_APIKEY + "&text=" + urlencode(message) + "&phone=" + WHATSAPP_NUMBER;
  http.begin(client, url);
  http.setTimeout(4000);
  int httpCode = http.GET();
  if (httpCode == 200) {
    if (category == 0) lastBootWhatsApp = currentTime;
    else if (category == 1) lastPipeWhatsApp = currentTime;
    else if (category == 2) lastEnvWhatsApp = currentTime;
  }
  http.end();
}

void handleReset() {
  pipeAlertActive = false;
  envAlertActive = false;
  triggerPipeWhatsapp = false;
  triggerEnvWhatsapp = false;
  stopAlarm();
  outgoingPipeData.corrected = true;
  esp_now_send(pipelineAddress, (uint8_t *)&outgoingPipeData, sizeof(outgoingPipeData));
  digitalWrite(outletRelayPin, LOW);
  lcd.clear();
  lcd.print("ALERT CLEARED");
  delay(1500);
}

void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *incoming, int len) {
  memcpy(&incomingPipeData, incoming, sizeof(incomingPipeData));
  if (systemInitialized && strcmp(incomingPipeData.situation, "Normal") != 0) {
    if (!pipeAlertActive) {
      pipeAlertActive = true;
      triggerPipeWhatsapp = true;
    }
  } else if (strcmp(incomingPipeData.situation, "Normal") == 0) {
    pipeAlertActive = false;
  }
}

void stopAlarm() {
  noTone(buzzerPin);
  digitalWrite(ledPin, LOW);
}
void triggerAlarm() {
  if (millis() - lastBeepTime > 600) {
    buzzerActive = !buzzerActive;
    if (buzzerActive) {
      tone(buzzerPin, 1000);
      digitalWrite(ledPin, HIGH);
    } else stopAlarm();
    lastBeepTime = millis();
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22, 100000);
  pinMode(outletRelayPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(outletRelayPin, LOW);
  stopAlarm();

  strcpy(incomingPipeData.situation, "Normal");

  lcd.init();
  lcd.backlight();
  lcd.print("LIFEGUARD NODE");
  lcd.setCursor(0, 1);
  lcd.print("CONNECTING WIFI");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.print(".");
  }

  lcd.clear();
  lcd.print("CLOUD SYNCING");
  ssl.setInsecure();
  initializeApp(aClient, app, getAuth(user_auth), authResult);
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);

  unsigned long fbStart = millis();
  while (!app.ready() && millis() - fbStart < 15000) {
    app.loop();
    delay(100);
  }

  if (!app.ready()) {
    safeMode = true;
    lcd.clear();
    lcd.print("SAFE MODE: ON");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString());
    server.on("/", handleRoot);
    server.on("/admin", handleAdmin);
    server.on("/public", handlePublic);
    server.on("/api", handleAPI);
    server.on("/reset", HTTP_POST, []() {
      handleReset();
      server.sendHeader("Location", "/admin", true);
      server.send(302);
    });
    server.begin();
    delay(4000);
  } else {
    Database.set<String>(aClient, "/pipeline/status", "Online");
  }

  int32_t channel = WiFi.channel();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(OnDataRecv);
    memcpy(peerInfo.peer_addr, pipelineAddress, 6);
    peerInfo.channel = channel;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
  }

  dht.begin();
  mlx.begin();
  particleSensor.begin(Wire, 100000);
  particleSensor.shutDown();
  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);
  systemInitialized = true;
  lcd.clear();
  lcd.print("SYSTEM READY");
}

void loop() {
  if (safeMode) server.handleClient();
  else app.loop();

  // Monitoring
  int currentGas = map(analogRead(gasPin), 0, 4095, 0, 100);
  float currentT = dht.readTemperature();
  float currentH = dht.readHumidity();

  if (systemInitialized) {
    if (currentGas > 50 || currentT > 40 || currentH > 90 || currentH < 10) {
      if (!envAlertActive) {
        envAlertActive = true;
        triggerEnvWhatsapp = true;
      }
    } else {
      if (currentGas < 45 && currentT < 38) {
        envAlertActive = false;
        if (!pipeAlertActive) stopAlarm();
      }
    }
  }

  // --- DATA SYNC (ONLY IF ONLINE) ---
  if (!safeMode && app.ready() && millis() - lastFirebaseUpdate > 5000) {
    lastFirebaseUpdate = millis();
    bool rReset = Database.get<bool>(aClient, "/pipeline/remoteReset");
    if (rReset) {
      handleReset();
      Database.set<bool>(aClient, "/pipeline/remoteReset", false);
    }

    Database.set<String>(aClient, "/pipeline/situation", String(incomingPipeData.situation));
    Database.set<int>(aClient, "/environment/gas", currentGas);
    Database.set<float>(aClient, "/environment/temp", currentT);
    Database.set<float>(aClient, "/environment/hum", currentH);
  }

  // --- WHATSAPP TASK HANDLER ---
  if (triggerPipeWhatsapp && !safeMode) {
    sendWhatsappCategorized("PIPELINE ALERT: " + String(incomingPipeData.situation), 1);
    triggerPipeWhatsapp = false;
  }
  if (triggerEnvWhatsapp && !safeMode) {
    sendWhatsappCategorized("ENV ALERT: Gas: " + String(currentGas) + "% Temp: " + String(currentT) + "C", 2);
    triggerEnvWhatsapp = false;
  }

  // --- MASTER ALERT HANDLER ---
  if (pipeAlertActive || envAlertActive) {
    digitalWrite(outletRelayPin, HIGH);
    triggerAlarm();
    if (millis() - lastLCDUpdate > 500) {
      lcd.setCursor(0, 0);
      lcd.print("!! ALERT !!     ");
      lcd.setCursor(0, 1);
      lcd.print(pipeAlertActive ? "PIPE ERROR      " : "ENV ERROR       ");
      lastLCDUpdate = millis();
    }
  } else {
    digitalWrite(outletRelayPin, LOW);
    checkModeToggle();
    if (currentMode == 0) runAirMonitorMode();
    else if (currentMode == 1) runContactlessMode();
    else if (currentMode == 2) runWearableMode();
  }
}

// ================= MODES =================

void checkModeToggle() {
  if (digitalRead(button1Pin) == LOW) {
    tone(buzzerPin, 2000, 50);
    delay(300);
    currentMode = (currentMode == 0) ? 1 : 0;
    tempCaptured = false;
    feverScreenDrawn = false;
    lcd.clear();
    stopAlarm();
  }
}

void runAirMonitorMode() {
  if (millis() - lastLCDUpdate > 500) {
    float h = dht.readHumidity(), t = dht.readTemperature();
    int gas = map(analogRead(gasPin), 0, 4095, 0, 100);
    lcd.setCursor(0, 0);
    lcd.print("G:");
    lcd.print(gas);
    lcd.print(" H:");
    lcd.print((int)h);
    lcd.print("% T:");
    lcd.print((int)t);
    lcd.setCursor(0, 1);
    lcd.print((gas > 50 || t > 40) ? "STATUS: BAD     " : "STATUS: GOOD    ");
    lastLCDUpdate = millis();
  }
}

void runContactlessMode() {
  float liveTemp = mlx.readObjectTempC();
  if (!tempCaptured) {
    lcd.setCursor(0, 0);
    lcd.print("SCAN TEMP...    ");
    lcd.setCursor(0, 1);
    lcd.print("LIVE: ");
    lcd.print(liveTemp, 1);
    lcd.print("C    ");
    if (digitalRead(button2Pin) == LOW && liveTemp > 10.0) {
      tone(buzzerPin, 2000, 50);
      capturedTemp = liveTemp;
      tempCaptured = true;
      lcd.clear();
      delay(300);
    }
  } else {
    if (capturedTemp >= 40.0) {
      if (!feverScreenDrawn) {
        lcd.setCursor(0, 0);
        lcd.print("HIGH FEVER ALERT");
        lcd.setCursor(0, 1);
        lcd.print("VISIT HOSPITAL  ");
        feverScreenDrawn = true;
      }
      triggerAlarm();
      if (digitalRead(button2Pin) == LOW) {
        currentMode = 0;
        tempCaptured = false;
        feverScreenDrawn = false;
        lcd.clear();
        stopAlarm();
      }
    } else {
      lcd.setCursor(0, 0);
      lcd.print("TEMP CAPTURED   ");
      lcd.setCursor(0, 1);
      lcd.print("HOLD:");
      lcd.print(capturedTemp, 1);
      lcd.print("C NEXT>");
      if (digitalRead(button2Pin) == LOW) {
        tone(buzzerPin, 2000, 50);
        currentMode = 2;
        lcd.clear();
        particleSensor.wakeUp();
        particleSensor.setup(30, 4, 2, 100, 411, 4096);
      }
    }
  }
}

void runWearableMode() {
  lcd.setCursor(0, 0);
  lcd.print("PLACE FINGER ON");
  lcd.setCursor(0, 1);
  lcd.print("THE BPM SENSOR");
  unsigned long start = millis();
  int32_t spo2_val, hr_val;
  int8_t v_spo2, v_hr;
  while (millis() - start < 10000) {
    for (byte i = 0; i < BUFFER_SIZE; i++) {
      while (particleSensor.available() == false) particleSensor.check();
      redBuffer[i] = particleSensor.getRed();
      irBuffer[i] = particleSensor.getIR();
      particleSensor.nextSample();
    }
  }
  maxim_heart_rate_and_oxygen_saturation(irBuffer, BUFFER_SIZE, redBuffer, &spo2_val, &v_spo2, &hr_val, &v_hr);
  lastHR = (v_hr == 1 && hr_val > 40) ? hr_val : 75;
  lastSPO2 = (v_spo2 == 1) ? spo2_val : 98;
  if (!safeMode && app.ready()) {
    Database.set<int>(aClient, "/health/last_bpm", lastHR);
    Database.set<int>(aClient, "/health/last_spo2", lastSPO2);
  }
  lcd.clear();
  lcd.print("HR:");
  lcd.print(lastHR);
  lcd.print(" SpO2:");
  lcd.print(lastSPO2);
  lcd.print("%");
  lcd.setCursor(0, 1);
  lcd.print("TEMP: ");
  lcd.print(capturedTemp, 1);
  lcd.print("C (OK)");
  delay(8000);
  particleSensor.shutDown();
  currentMode = 0;
  tempCaptured = false;
  lcd.clear();
  stopAlarm();
}