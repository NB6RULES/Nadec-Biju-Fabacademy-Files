/*
 * esp32_keyboard.ino — XIAO ESP32-C6 Wireless QWERTY Keyboard
 *
 * Board  : Seeed XIAO ESP32C6 (esp32 core 3.x)
 * OLED   : SSD1306 128×64 via I2C — SDA: D4, SCL: D5
 * Buttons: UP=D3, DOWN=D9, LEFT=D2, RIGHT=D7, SPACE=D6, SEND=D1
 *          All INPUT_PULLUP, active LOW
 *
 * Screens (cycle with long-press SEND ≥1000 ms):
 *   0 — QWERTY + numbers keyboard + text buffer
 *   1 — Client select  (appears on short-press SEND from keyboard)
 *   2 — IP / status
 *
 * Clients:
 *   Pico W  → 192.168.4.2:4210
 *   Pico 2W → 192.168.4.3:4210
 *
 * Web: serves HTML keyboard on port 80 at 192.168.4.1
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUDP.h>

// ─── Pin Definitions ─────────────────────────────────────────
#define BTN_UP    D3
#define BTN_DOWN  D9
#define BTN_LEFT  D2
#define BTN_RIGHT D7
#define BTN_SPACE D6
#define BTN_SEND  D1
#define OLED_SDA  D4
#define OLED_SCL  D5

// ─── Network Configuration ───────────────────────────────────
#define AP_SSID   "ESP_KB"
#define AP_PASS   "12345678"
#define STA_SSID  ""
#define STA_PASS  ""

#define PICO_IP    "192.168.4.2"   // Pico W
#define PICO2_IP   "192.168.4.3"   // Pico 2W
#define PICO_PORT  4210

// ─── OLED ────────────────────────────────────────────────────
#define SCREEN_W  128
#define SCREEN_H  64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

// ─── Web Server & UDP ────────────────────────────────────────
WebServer server(80);
WiFiUDP   udp;

// ─── Button indices ───────────────────────────────────────────
#define BTN_IDX_UP    0
#define BTN_IDX_DOWN  1
#define BTN_IDX_LEFT  2
#define BTN_IDX_RIGHT 3
#define BTN_IDX_SPACE 4
#define BTN_IDX_SEND  5
#define NUM_BTNS      6

const uint8_t BTN_PINS[NUM_BTNS] = {
  BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_SPACE, BTN_SEND
};

struct BtnState {
  bool      lastRaw;
  bool      pressEvent;
  bool      releaseEvent;
  bool      isHeld;
  unsigned long debounceTime;
  unsigned long pressTime;
};
BtnState btns[NUM_BTNS];

#define DEBOUNCE_MS 50

// ─── Keyboard Layout ─────────────────────────────────────────
// 5 rows: numbers, QWERTY, ASDF, ZXC, special
#define KEY_CAPS  '\x01'
#define KEY_BKSP  '\x02'
#define KEY_SEND  '\x03'

#define KB_NUM_ROWS 5

const char* KB_ROWS[KB_NUM_ROWS] = {
  "1234567890",    // row 0
  "QWERTYUIOP",   // row 1
  "ASDFGHJKL",    // row 2
  "ZXCVBNM",      // row 3
  "\x01\x02\x03"  // row 4: CAPS, BKSP, SEND
};
const int KB_ROW_LEN[KB_NUM_ROWS]   = {10, 10, 9, 7, 3};
// X offset so each row appears centred
const int KB_ROW_OFF_X[KB_NUM_ROWS] = {4, 4, 10, 22, 8};
// Y pixel top of each row — 8px per row to fit all 5 in ~40px
const int KB_ROW_Y[KB_NUM_ROWS]     = {0, 8, 16, 24, 32};

#define KEY_CELL_W   12
const int SPECIAL_W[3] = {30, 36, 40};  // CAP, BKS, SND

// ─── Keyboard cursor state ───────────────────────────────────
int  curRow = 1;   // default to QWERTY row (row 1)
int  curCol = 0;
bool capsOn  = false;

// ─── Text buffer ─────────────────────────────────────────────
#define TEXT_BUF_MAX 200
char textBuf[TEXT_BUF_MAX + 1];
int  textLen = 0;

// ─── Screen state ────────────────────────────────────────────
enum Screen {
  SCR_KEYBOARD      = 0,
  SCR_CLIENT_SELECT = 1,   // shown after short-press SEND on keyboard
  SCR_STATUS        = 2
};
Screen currentScreen = SCR_KEYBOARD;
bool   displayDirty  = true;

// ─── Client selection ────────────────────────────────────────
// 0 = Pico W (192.168.4.2), 1 = Pico 2W (192.168.4.3)
int selectedClient = 0;

// ─── Last sent string (for status display) ──────────────────
char lastSent[64] = "";

// ─── Double-press detection for SPACE ───────────────────────
unsigned long spaceFirstTap = 0;
bool          spaceWaiting  = false;
#define DOUBLE_TAP_WINDOW_MS 300

// ─── Long-press detection for SEND ──────────────────────────
bool sendLongFired = false;

// ─── Inline HTML page ────────────────────────────────────────
static const char HTML_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP Keyboard</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:sans-serif;background:#111;color:#eee;display:flex;
       flex-direction:column;align-items:center;padding:20px;gap:16px}
  h1{font-size:1.4rem;letter-spacing:2px}
  textarea{width:100%;max-width:480px;height:160px;font-size:1.1rem;
           padding:10px;border-radius:8px;border:1px solid #444;
           background:#222;color:#fff;resize:vertical}
  .client-row{width:100%;max-width:480px;display:flex;gap:12px}
  .client-btn{flex:1;padding:12px;font-size:1rem;border:2px solid #444;
              border-radius:8px;background:#222;color:#aaa;cursor:pointer;
              text-align:center;transition:all .15s}
  .client-btn.active{border-color:#0af;color:#0af;background:#0a1a22}
  button{width:100%;max-width:480px;padding:16px;font-size:1.2rem;
         background:#0af;border:none;border-radius:8px;color:#000;
         font-weight:bold;cursor:pointer}
  button:active{background:#08c}
  #status{width:100%;max-width:480px;font-size:.85rem;color:#aaa;
          background:#1a1a1a;padding:8px 12px;border-radius:6px;
          word-break:break-all}
</style>
</head>
<body>
<h1>ESP KEYBOARD</h1>
<textarea id="msg" placeholder="Type text to send…"></textarea>
<div class="client-row">
  <div class="client-btn active" id="cb0" onclick="selectClient(0)">Pico W<br><small>192.168.4.2</small></div>
  <div class="client-btn"        id="cb1" onclick="selectClient(1)">Pico 2W<br><small>192.168.4.3</small></div>
</div>
<button onclick="sendText()">Send</button>
<div id="status">Waiting for status…</div>

<script>
var activeClient=0;
function selectClient(n){
  activeClient=n;
  document.getElementById('cb0').className='client-btn'+(n===0?' active':'');
  document.getElementById('cb1').className='client-btn'+(n===1?' active':'');
}
function sendText(){
  const msg=document.getElementById('msg').value;
  if(!msg)return;
  fetch('/send',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
        body:'msg='+encodeURIComponent(msg)+'&client='+activeClient})
    .then(r=>r.text())
    .then(()=>{document.getElementById('msg').value='';fetchStatus();})
    .catch(e=>console.error(e));
}
function fetchStatus(){
  fetch('/status').then(r=>r.json()).then(d=>{
    document.getElementById('status').textContent=
      'Last sent: '+d.last_sent+' | Clients: '+d.clients;
  }).catch(()=>{});
}
setInterval(fetchStatus,2000);
fetchStatus();
</script>
</body>
</html>
)rawhtml";

// ─── Forward declarations ────────────────────────────────────
void readButtons();
bool btnJustPressed(int idx);
bool btnJustReleased(int idx);
bool btnHeld(int idx, unsigned long ms);

void handleSpaceButton();
void handleSendButton();
void handleNavButtons();

void selectCurrentKey();
void appendChar(char c);
void doBackspace();
void sendText(int clientIdx);
void toggleCaps();
void openClientSelect();

void drawScreen();
void drawKeyboardScreen();
void drawClientSelectScreen();
void drawStatusScreen();
void drawKeyRow(int row);
int  specialKeyX(int col);

void setupWiFiAP();
void setupWebServer();
void sendUDP(const char* text, int len, const char* destIP);

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("[OLED] Init failed");
    for (;;) delay(100);
  }
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(0xFF);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 20);
  display.println("ESP Keyboard");
  display.setCursor(20, 32);
  display.println("Starting…");
  display.display();

  for (int i = 0; i < NUM_BTNS; i++) {
    pinMode(BTN_PINS[i], INPUT_PULLUP);
    btns[i] = {false, false, false, false, 0, 0};
  }

  memset(textBuf, 0, sizeof(textBuf));

  setupWiFiAP();
  setupWebServer();
  udp.begin(4211);

  displayDirty = true;
  Serial.println("[SYS] Setup complete");
}

// ─────────────────────────────────────────────────────────────
void loop() {
  readButtons();
  handleSpaceButton();
  handleSendButton();
  handleNavButtons();
  server.handleClient();

  if (displayDirty) {
    drawScreen();
    displayDirty = false;
  }
}

// ─── Button debounce & edge detection ───────────────────────
void readButtons() {
  unsigned long now = millis();
  for (int i = 0; i < NUM_BTNS; i++) {
    bool raw = (digitalRead(BTN_PINS[i]) == LOW);

    if (raw != btns[i].lastRaw) {
      btns[i].debounceTime = now;
      btns[i].lastRaw      = raw;
    }

    if ((now - btns[i].debounceTime) >= DEBOUNCE_MS) {
      bool wasHeld = btns[i].isHeld;
      btns[i].isHeld = raw;

      if (raw && !wasHeld) {
        btns[i].pressEvent   = true;
        btns[i].releaseEvent = false;
        btns[i].pressTime    = now;
      } else if (!raw && wasHeld) {
        btns[i].releaseEvent = true;
        btns[i].pressEvent   = false;
      } else {
        btns[i].pressEvent   = false;
        btns[i].releaseEvent = false;
      }
    } else {
      btns[i].pressEvent   = false;
      btns[i].releaseEvent = false;
    }
  }
}

bool btnJustPressed(int idx)  { return btns[idx].pressEvent; }
bool btnJustReleased(int idx) { return btns[idx].releaseEvent; }
bool btnHeld(int idx, unsigned long ms) {
  return btns[idx].isHeld && (millis() - btns[idx].pressTime >= ms);
}

// ─── SPACE: double-press on keyboard / confirm on client select ──
void handleSpaceButton() {
  if (currentScreen == SCR_CLIENT_SELECT) {
    if (btnJustPressed(BTN_IDX_SPACE)) {
      sendText(selectedClient);
      currentScreen = SCR_KEYBOARD;
      displayDirty  = true;
    }
    return;
  }

  if (currentScreen != SCR_KEYBOARD) return;

  unsigned long now = millis();

  if (btnJustPressed(BTN_IDX_SPACE)) {
    if (spaceWaiting && (now - spaceFirstTap < DOUBLE_TAP_WINDOW_MS)) {
      appendChar(' ');
      spaceWaiting = false;
      displayDirty = true;
    } else {
      spaceWaiting  = true;
      spaceFirstTap = now;
    }
  }

  if (spaceWaiting && (now - spaceFirstTap >= DOUBLE_TAP_WINDOW_MS)) {
    spaceWaiting = false;
    selectCurrentKey();
    displayDirty = true;
  }
}

// ─── SEND button handler ─────────────────────────────────────
void handleSendButton() {
  if (btnJustPressed(BTN_IDX_SEND)) {
    sendLongFired = false;
  }

  // Long-press: toggle between KB and STATUS (2 screens only)
  if (!sendLongFired && btnHeld(BTN_IDX_SEND, 1000)) {
    sendLongFired = true;
    if (currentScreen == SCR_STATUS) {
      currentScreen = SCR_KEYBOARD;
    } else {
      currentScreen = SCR_STATUS;
    }
    displayDirty = true;
    Serial.print("[UI] Screen → ");
    Serial.println(currentScreen);
  }

  // Short press on release
  if (btnJustReleased(BTN_IDX_SEND) && !sendLongFired) {
    if (currentScreen == SCR_KEYBOARD) {
      if (textLen > 0) openClientSelect();
    } else if (currentScreen == SCR_CLIENT_SELECT) {
      sendText(selectedClient);
      currentScreen = SCR_KEYBOARD;
    }
    displayDirty = true;
  }
}

// ─── Navigation buttons ───────────────────────────────────────
void handleNavButtons() {
  if (currentScreen == SCR_CLIENT_SELECT) {
    if (btnJustPressed(BTN_IDX_UP) || btnJustPressed(BTN_IDX_DOWN)) {
      selectedClient = 1 - selectedClient;
      displayDirty   = true;
    }
    if (btnJustPressed(BTN_IDX_LEFT)) {
      currentScreen = SCR_KEYBOARD;
      displayDirty  = true;
    }
    return;
  }

  if (currentScreen != SCR_KEYBOARD) return;

  bool moved = false;

  if (btnJustPressed(BTN_IDX_LEFT)) {
    curCol--;
    if (curCol < 0) curCol = KB_ROW_LEN[curRow] - 1;
    moved = true;
  }
  if (btnJustPressed(BTN_IDX_RIGHT)) {
    curCol++;
    if (curCol >= KB_ROW_LEN[curRow]) curCol = 0;
    moved = true;
  }
  if (btnJustPressed(BTN_IDX_UP)) {
    curRow--;
    if (curRow < 0) curRow = KB_NUM_ROWS - 1;
    if (curCol >= KB_ROW_LEN[curRow]) curCol = KB_ROW_LEN[curRow] - 1;
    moved = true;
  }
  if (btnJustPressed(BTN_IDX_DOWN)) {
    curRow++;
    if (curRow >= KB_NUM_ROWS) curRow = 0;
    if (curCol >= KB_ROW_LEN[curRow]) curCol = KB_ROW_LEN[curRow] - 1;
    moved = true;
  }

  if (moved) displayDirty = true;
}

// ─── Key selection ────────────────────────────────────────────
void selectCurrentKey() {
  char k = KB_ROWS[curRow][curCol];

  if (k == KEY_CAPS) {
    toggleCaps();
  } else if (k == KEY_BKSP) {
    doBackspace();
  } else if (k == KEY_SEND) {
    if (textLen > 0) openClientSelect();
  } else {
    // Numbers are unaffected by caps; letters obey caps
    char c = (k >= 'A' && k <= 'Z') ? (capsOn ? toupper(k) : tolower(k)) : k;
    appendChar(c);
  }
}

void appendChar(char c) {
  if (textLen < TEXT_BUF_MAX) {
    textBuf[textLen++] = c;
    textBuf[textLen]   = '\0';
    displayDirty = true;
  }
}

void doBackspace() {
  if (textLen > 0) {
    textBuf[--textLen] = '\0';
    displayDirty = true;
  }
}

void openClientSelect() {
  selectedClient = 0;
  currentScreen  = SCR_CLIENT_SELECT;
  displayDirty   = true;
}

void sendText(int clientIdx) {
  if (textLen == 0) return;
  const char* destIP = (clientIdx == 0) ? PICO_IP : PICO2_IP;
  sendUDP(textBuf, textLen, destIP);

  strncpy(lastSent, textBuf, sizeof(lastSent) - 1);
  lastSent[sizeof(lastSent) - 1] = '\0';

  memset(textBuf, 0, sizeof(textBuf));
  textLen      = 0;
  displayDirty = true;

  Serial.print("[SEND→");
  Serial.print((clientIdx == 0) ? "PicoW" : "Pico2W");
  Serial.print("] ");
  Serial.println(lastSent);
}

void toggleCaps() {
  capsOn       = !capsOn;
  displayDirty = true;
}

// ─── UDP transmission ────────────────────────────────────────
void sendUDP(const char* text, int len, const char* destIP) {
  IPAddress dest;
  dest.fromString(destIP);
  udp.beginPacket(dest, PICO_PORT);
  udp.write((const uint8_t*)text, len);
  udp.write((const uint8_t*)"\n", 1);
  udp.endPacket();
}

// ─── WiFi setup ───────────────────────────────────────────────
void setupWiFiAP() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("[WiFi] AP started — IP: ");
  Serial.println(WiFi.softAPIP());

  if (strlen(STA_SSID) > 0) {
    Serial.print("[WiFi] Connecting STA to ");
    Serial.println(STA_SSID);
    WiFi.begin(STA_SSID, STA_PASS);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(200);
      Serial.print('.');
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("[WiFi] STA IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("[WiFi] STA failed — AP-only mode");
    }
  }
}

// ─── Web server ───────────────────────────────────────────────
void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", HTML_PAGE);
  });

  server.on("/send", HTTP_POST, []() {
    if (server.hasArg("msg")) {
      String msg = server.arg("msg");

      int clientIdx = 0;
      if (server.hasArg("client")) {
        clientIdx = server.arg("client").toInt();
        if (clientIdx < 0 || clientIdx > 1) clientIdx = 0;
      }
      const char* destIP = (clientIdx == 0) ? PICO_IP : PICO2_IP;

      sendUDP(msg.c_str(), msg.length(), destIP);
      strncpy(lastSent, msg.c_str(), sizeof(lastSent) - 1);
      lastSent[sizeof(lastSent) - 1] = '\0';
      displayDirty = true;
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Missing msg");
    }
  });

  server.on("/status", HTTP_GET, []() {
    int clients = WiFi.softAPgetStationNum();
    String json = "{\"last_sent\":\"";
    for (int i = 0; lastSent[i]; i++) {
      if (lastSent[i] == '"') json += '\\';
      json += lastSent[i];
    }
    json += "\",\"clients\":";
    json += clients;
    json += "}";
    server.send(200, "application/json", json);
  });

  server.begin();
  Serial.println("[Web] Server started on port 80");
}

// ─── OLED drawing ────────────────────────────────────────────
void drawScreen() {
  display.clearDisplay();
  switch (currentScreen) {
    case SCR_KEYBOARD:      drawKeyboardScreen();     break;
    case SCR_CLIENT_SELECT: drawClientSelectScreen(); break;
    case SCR_STATUS:        drawStatusScreen();       break;
  }
  display.display();
}

static int keyX(int row, int col) {
  return KB_ROW_OFF_X[row] + col * KEY_CELL_W;
}

int specialKeyX(int col) {
  // [CAP 30px][1px][BKS 36px][1px][SND 40px] total=108px, margin=(128-108)/2=10
  const int margin = 10;
  if (col == 0) return margin;
  if (col == 1) return margin + SPECIAL_W[0] + 1;
  return margin + SPECIAL_W[0] + 1 + SPECIAL_W[1] + 1;
}

void drawKeyRow(int row) {
  for (int col = 0; col < KB_ROW_LEN[row]; col++) {
    bool selected = (row == curRow && col == curCol);
    char k        = KB_ROWS[row][col];
    int  y        = KB_ROW_Y[row];

    if (row < KB_NUM_ROWS - 1) {
      // Standard key (number or letter)
      int x = keyX(row, col);
      if (selected) {
        display.fillRect(x, y, KEY_CELL_W - 1, 7, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else {
        display.setTextColor(SSD1306_WHITE);
      }
      char label[2];
      if (k >= 'A' && k <= 'Z') {
        label[0] = capsOn ? toupper(k) : tolower(k);
      } else {
        label[0] = k;   // numbers stay as-is
      }
      label[1] = '\0';
      display.setCursor(x + 3, y + 1);
      display.print(label);
      display.setTextColor(SSD1306_WHITE);
    } else {
      // Special key (last row)
      int x = specialKeyX(col);
      int w = SPECIAL_W[col];
      const char* labels[] = {"CAP", "BKS", "SND"};

      if (selected) {
        display.fillRect(x, y, w - 1, 7, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else {
        display.drawRect(x, y, w - 1, 7, SSD1306_WHITE);
        display.setTextColor(SSD1306_WHITE);
      }
      int labelLen = strlen(labels[col]);
      int labelX   = x + (w - 1 - labelLen * 6) / 2;
      display.setCursor(labelX, y + 1);
      display.print(labels[col]);
      display.setTextColor(SSD1306_WHITE);
    }
  }
}

// ── Screen 0: keyboard (numbers + QWERTY) ────────────────────
//
//  Y= 0  1 2 3 4 5 6 7 8 9 0     (numbers)
//  Y= 8  Q W E R T Y U I O P
//  Y=16  A S D F G H J K L
//  Y=24  Z X C V B N M
//  Y=32  [CAP]  [BKS]  [SND]
//  Y=40  ─────────────────────   separator
//  Y=42  text buffer
//  Y=55  Len:xx [CAP]
//
void drawKeyboardScreen() {
  for (int r = 0; r < KB_NUM_ROWS; r++) drawKeyRow(r);

  // CAPS indicator top-right
  if (capsOn) {
    display.setCursor(110, 0);
    display.setTextColor(SSD1306_WHITE);
    display.print("CAP");
  }

  // Separator
  display.drawFastHLine(0, 40, 128, SSD1306_WHITE);

  // Text buffer — show last 21 chars
  display.setCursor(0, 42);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  if (textLen <= 21) {
    display.print(textBuf);
  } else {
    display.print(textBuf + textLen - 21);
  }

  // Status line
  display.setCursor(0, 55);
  display.setTextColor(SSD1306_WHITE);
  display.print("Len:");
  display.print(textLen);
  if (capsOn) display.print(" CAP");
}

// ── Screen 1: Client select ──────────────────────────────────
//
//  Y= 2  "  Send to:"
//  Y=13  ─────────────────────
//  Y=17  [  Pico W   ]          (filled when selectedClient==0)
//  Y=34  [  Pico 2W  ]          (filled when selectedClient==1)
//  Y=51  "^v:sel  SND/SPC:ok  L:bk"
//
void drawClientSelectScreen() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Title
  display.setCursor(40, 2);
  display.print("Send to:");

  display.drawFastHLine(0, 13, 128, SSD1306_WHITE);

  // ── Option 0: Pico W ───────────────────────────────────────
  if (selectedClient == 0) {
    display.fillRect(4, 17, 120, 13, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.drawRect(4, 17, 120, 13, SSD1306_WHITE);
    display.setTextColor(SSD1306_WHITE);
  }
  // Centre "Pico W" (6 chars × 6px = 36px)  →  x = (128-36)/2 = 46
  display.setCursor(46, 21);
  display.print("Pico W");
  display.setTextColor(SSD1306_WHITE);

  // ── Option 1: Pico 2W ──────────────────────────────────────
  if (selectedClient == 1) {
    display.fillRect(4, 34, 120, 13, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.drawRect(4, 34, 120, 13, SSD1306_WHITE);
    display.setTextColor(SSD1306_WHITE);
  }
  // Centre "Pico 2W" (7 chars × 6px = 42px)  →  x = (128-42)/2 = 43
  display.setCursor(43, 38);
  display.print("Pico 2W");
  display.setTextColor(SSD1306_WHITE);

  // Hint
  display.setCursor(4, 51);
  display.print("^v:sel  SND/SPC:ok  L:bk");
}

// ── Screen 2: IP / Status ────────────────────────────────────
void drawStatusScreen() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("AP:  ");
  display.print(WiFi.softAPIP());

  display.setCursor(0, 12);
  if (WiFi.status() == WL_CONNECTED) {
    display.print("STA: ");
    display.print(WiFi.localIP());
  } else {
    display.print("STA: disconnected");
  }

  display.setCursor(0, 24);
  display.print("Clients: ");
  display.print(WiFi.softAPgetStationNum());

  display.setCursor(0, 36);
  display.print("Last: ");
  if (strlen(lastSent) > 0) {
    char trunc[20];
    strncpy(trunc, lastSent, 19);
    trunc[19] = '\0';
    display.print(trunc);
  } else {
    display.print("(none)");
  }

  display.setCursor(0, 48);
  display.print("Buf: ");
  display.print(textLen);
  display.print(" chars");

  display.setCursor(0, 56);
  display.print("Long-press SEND: keyboard");
}
