/*
 * pico_hid.ino — Raspberry Pi Pico W UDP-to-HID Bridge
 *
 * Board    : Raspberry Pi Pico W
 * Core     : arduino-pico by Earle Philhower
 * USB Stack: Adafruit TinyUSB  (Tools > USB Stack > Adafruit TinyUSB)
 *
 * Behaviour:
 *   - Connects to ESP_KB WiFi AP with static IP 192.168.4.2
 *   - Listens for UDP packets on port 4210
 *   - Types received text as USB HID keystrokes (20 ms / char)
 *   - Blinks onboard LED once per packet
 *   - Auto-reconnects every 5 s if WiFi drops
 */

#include <WiFi.h>
#include <WiFiUDP.h>
#include <Keyboard.h>   // Philhower core — requires TinyUSB stack

// ─── Configuration ────────────────────────────────────────────
#define LED_PIN     LED_BUILTIN
#define UDP_PORT    4210
#define AP_SSID     "ESP_KB"
#define AP_PASS     "12345678"
#define STATIC_IP   "192.168.4.2"
#define GATEWAY     "192.168.4.1"
#define SUBNET      "255.255.255.0"

#define CHAR_DELAY_MS       20      // ms between each typed character
#define RECONNECT_INTERVAL  5000    // ms between reconnect attempts
#define WIFI_TIMEOUT_MS     10000   // ms to wait for initial connection
#define LED_BLINK_MS        80      // LED on duration per blink

// ─── Globals ──────────────────────────────────────────────────
WiFiUDP udp;
char packetBuf[512];

unsigned long lastReconnectAttempt = 0;
bool          udpReady             = false;

// LED blink state (non-blocking)
bool          ledOn        = false;
unsigned long ledOnTime    = 0;

// ─── Forward declarations ────────────────────────────────────
void connectWiFi();
void typeText(const char* text, int len);
void blinkLED();
void updateLED();

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);                         // let USB serial settle

  // LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Init USB HID keyboard
  Keyboard.begin();
  Serial.println("[HID] Keyboard initialised");

  // Connect to AP
  connectWiFi();
}

// ─────────────────────────────────────────────────────────────
void loop() {
  // ── Auto-reconnect if WiFi dropped ──────────────────────────
  if (WiFi.status() != WL_CONNECTED) {
    udpReady = false;
    unsigned long now = millis();
    if (now - lastReconnectAttempt >= RECONNECT_INTERVAL) {
      lastReconnectAttempt = now;
      Serial.println("[WiFi] Disconnected — reconnecting…");
      connectWiFi();
    }
  }

  // ── Handle incoming UDP packets ─────────────────────────────
  if (udpReady) {
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
      int len = udp.read(packetBuf, sizeof(packetBuf) - 1);
      if (len > 0) {
        packetBuf[len] = '\0';

        Serial.print("[UDP] Received (");
        Serial.print(len);
        Serial.print(" bytes): ");
        Serial.println(packetBuf);

        blinkLED();
        typeText(packetBuf, len);
      }
    }
  }

  // ── Non-blocking LED blink update ───────────────────────────
  updateLED();
}

// ─── WiFi connection ─────────────────────────────────────────
void connectWiFi() {
  Serial.print("[WiFi] Connecting to ");
  Serial.println(AP_SSID);

  // Set static IP before connecting
  IPAddress ip, gw, sn;
  ip.fromString(STATIC_IP);
  gw.fromString(GATEWAY);
  sn.fromString(SUBNET);
  WiFi.config(ip, gw, sn);

  WiFi.begin(AP_SSID, AP_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_TIMEOUT_MS) {
      Serial.println("[WiFi] Timeout — will retry later");
      return;
    }
    delay(200);
    Serial.print('.');
  }

  Serial.println();
  Serial.print("[WiFi] Connected, IP: ");
  Serial.println(WiFi.localIP());

  // (Re)open UDP socket — stop first to avoid socket leak on reconnect
  udp.stop();
  udp.begin(UDP_PORT);
  udpReady = true;
  Serial.print("[UDP] Listening on port ");
  Serial.println(UDP_PORT);
}

// ─── Type text via USB HID ───────────────────────────────────
void typeText(const char* text, int len) {
  // Strip trailing '\n' — but remember if a *second* newline is present
  // (spec: second \n = intentional Enter)
  int typeLen = len;

  // Remove a single trailing newline added by the sender
  if (typeLen > 0 && text[typeLen - 1] == '\n') {
    typeLen--;
  }

  // Type each character
  for (int i = 0; i < typeLen; i++) {
    char c = text[i];

    // Only type printable ASCII + basic control chars
    if ((c >= 32 && c <= 126) || c == '\n' || c == '\t') {
      Keyboard.write(c);    // Philhower Keyboard.write() handles shift internally
      delay(CHAR_DELAY_MS);
    }
    // Skip non-printable characters silently
  }

  Serial.println("[HID] Typing complete");
}

// ─── LED blink (non-blocking) ────────────────────────────────
void blinkLED() {
  digitalWrite(LED_PIN, HIGH);
  ledOn     = true;
  ledOnTime = millis();
}

void updateLED() {
  if (ledOn && (millis() - ledOnTime >= LED_BLINK_MS)) {
    digitalWrite(LED_PIN, LOW);
    ledOn = false;
  }
}
