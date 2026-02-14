#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <string.h>


#define MATRIX_PIN D10
#define BTN_UP D3
#define BTN_DOWN D9
#define BTN_LEFT D2
#define BTN_RIGHT D7
#define BTN_PAUSE D6
#define BUZZER D0
#define GAME_SELECTOR_BTN D1

constexpr uint8_t MATRIX_W = 8;
constexpr uint8_t MATRIX_H = 8;
constexpr uint8_t MATRIX_COUNT = 64;
constexpr uint16_t SCREEN_W = 128;
constexpr uint16_t SCREEN_H = 64;
constexpr uint8_t OLED_ADDR = 0x3C;
constexpr uint16_t BTN_DEBOUNCE_MS = 30;
constexpr uint16_t MATRIX_FRAME_MS = 33;
constexpr uint16_t OLED_FRAME_MS = 100;

Adafruit_NeoPixel matrix(MATRIX_COUNT, MATRIX_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, -1);

enum SystemState : uint8_t { STATE_MENU, STATE_PLAYING, STATE_GAME_OVER };

enum GameId : uint8_t {
  GAME_SNAKE_WALL,
  GAME_SNAKE_WRAP,
  GAME_TETRIS,
  GAME_FLAAY_EASY,
  GAME_FLAPPY_HARD,
  GAME_ASTEROIDS_HARD,
  GAME_PACMAN_EASY,
  GAME_PACMAN_HARD,
  GAME_SPACE_SHOOTER,
  GAME_BREAKOUT,
  GAME_TTT_AI,
  GAME_TTT_2P,
  GAME_PONG,
  GAME_TUG,
  GAME_COUNT
};

const char *GAME_NAMES[GAME_COUNT] = {
    "Snake (Wall)",    "Snake (Wrap)",    "Tetris",          "Flaay Bird Easy", "Flappy Bird Hard",
    "Asteroids Hard",  "Pac-Man Easy",    "Pac-Man Hard",    "Space Shooter",   "Breakout",
    "TicTacToe AI",    "TicTacToe 2P",    "Pong",            "Tug of War 2P"};

enum ButtonId : uint8_t { B_UP, B_DOWN, B_LEFT, B_RIGHT, B_PAUSE, B_SELECT, BUTTON_COUNT };
const uint8_t BUTTON_PINS[BUTTON_COUNT] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_PAUSE, GAME_SELECTOR_BTN};

struct Button {
  uint8_t pin;
  bool stable;
  bool last;
  bool press;
  bool release;
  uint32_t changedAt;
};
Button buttons[BUTTON_COUNT];

struct ToneEvent {
  uint16_t f;
  uint16_t d;
};
constexpr uint8_t TQ = 24;
ToneEvent toneQ[TQ];
uint8_t toneHead = 0;
uint8_t toneTail = 0;
bool toneOn = false;
uint32_t toneUntil = 0;

SystemState systemState = STATE_MENU;
GameId currentGame = GAME_SNAKE_WALL;

int8_t menuIndex = 0;
int16_t score = 0;
int16_t highScore[GAME_COUNT] = {0};
bool lastWin = false;
char lastMsg[24] = "Game over";









uint32_t frame[MATRIX_COUNT];
uint32_t lastMatrixMs = 0;
uint32_t lastOledMs = 0;

void initGame(GameId id);
void updateGame(GameId id, uint32_t now);
void drawGame(GameId id);

static inline uint32_t C(uint8_t r, uint8_t g, uint8_t b) { return matrix.Color(r, g, b); }













void enqueueTone(uint16_t f, uint16_t d) {
  uint8_t n = static_cast<uint8_t>((toneTail + 1) % TQ);
  if (n == toneHead) return;
  toneQ[toneTail] = {f, d};
  toneTail = n;
}
void beepBtn() { enqueueTone(1200, 26); }
void beepScore() { enqueueTone(1600, 35); }
void beepHit() { enqueueTone(240, 90); }
void beepStart() {
  enqueueTone(500, 45);
  enqueueTone(850, 45);
  enqueueTone(1200, 65);
}
void beepLose() {
  enqueueTone(1000, 60);
  enqueueTone(700, 70);
  enqueueTone(430, 90);
}
void beepWin() {
  enqueueTone(800, 50);
  enqueueTone(1050, 50);
  enqueueTone(1400, 80);
}

void updateSound(uint32_t now) {
  if (toneOn && now >= toneUntil) {
    noTone(BUZZER);
    toneOn = false;
  }
  if (!toneOn && toneHead != toneTail) {

    ToneEvent ev = toneQ[toneHead];  
    toneHead = (toneHead + 1) % TQ;   

  
    tone(BUZZER, ev.f);
    toneUntil = now + ev.d;
    toneOn = true;
  }
}

void initButtons() {
  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    bool p = digitalRead(BUTTON_PINS[i]) == LOW;
    buttons[i] = {BUTTON_PINS[i], p, p, false, false, millis()};
  }
}
void clearEdges() {
  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) buttons[i].press = buttons[i].release = false;
}
void updateButtons(uint32_t now) {
  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
    bool r = digitalRead(buttons[i].pin) == LOW;
    if (r != buttons[i].last) {
      buttons[i].last = r;
      buttons[i].changedAt = now;
    }
    if (now - buttons[i].changedAt >= BTN_DEBOUNCE_MS && r != buttons[i].stable) {
      buttons[i].stable = r;
      if (r)
        buttons[i].press = true;
      else
        buttons[i].release = true;
    }
  }
}
bool take(ButtonId id) {
  if (!buttons[id].press) return false;
  buttons[id].press = false;
  return true;
}
bool down(ButtonId id) { return buttons[id].stable; }

uint8_t idx(uint8_t x, uint8_t y) { return (y % 2 == 0) ? static_cast<uint8_t>(y * MATRIX_W + x) : static_cast<uint8_t>(y * MATRIX_W + (MATRIX_W - 1 - x)); }
void clearFrame(uint32_t color = 0) {
  for (uint8_t i = 0; i < MATRIX_COUNT; ++i) frame[i] = color;
}
void px(int8_t x, int8_t y, uint32_t c) {
  if (x < 0 || x >= MATRIX_W || y < 0 || y >= MATRIX_H) return;
  frame[idx(static_cast<uint8_t>(x), static_cast<uint8_t>(y))] = c;
}
void showFrame() {
  for (uint8_t i = 0; i < MATRIX_COUNT; ++i) matrix.setPixelColor(i, frame[i]);
  matrix.show();
}

void backToMenu() {
  systemState = STATE_MENU;

  clearEdges();
}
void finish(bool win, const char *msg) {
  if (systemState != STATE_PLAYING) return;
  if (score > highScore[currentGame]) highScore[currentGame] = score;
  lastWin = win;
  strncpy(lastMsg, msg ? msg : (win ? "Win" : "Game over"), sizeof(lastMsg) - 1);
  lastMsg[sizeof(lastMsg) - 1] = '\0';
  systemState = STATE_GAME_OVER;
  clearEdges();
  if (win)
    beepWin();
  else
    beepLose();
}
void startGame(GameId id) {
  currentGame = id;
  score = 0;
  lastWin = false;
  strncpy(lastMsg, "Game over", sizeof(lastMsg) - 1);
  systemState = STATE_PLAYING;

  clearEdges();
  initGame(id);
  beepStart();
}

// ---- game state storage + implementations are inserted below ----
struct Bullet {
  int8_t x;
  int8_t y;
  bool on;
};

struct Snake {
  int8_t x[64];
  int8_t y[64];
  uint8_t n;
  int8_t dx;
  int8_t dy;
  int8_t ndx;
  int8_t ndy;
  int8_t fx;
  int8_t fy;
  bool wrap;
  uint16_t ms;
  uint32_t last;
} snake;

bool snakeHas(int8_t x, int8_t y, uint8_t len) {
  for (uint8_t i = 0; i < len; ++i)
    if (snake.x[i] == x && snake.y[i] == y) return true;
  return false;
}
void snakeFood() {
  for (uint8_t i = 0; i < 100; ++i) {
    int8_t x = static_cast<int8_t>(random(0, 8));
    int8_t y = static_cast<int8_t>(random(0, 8));
    if (!snakeHas(x, y, snake.n)) {
      snake.fx = x;
      snake.fy = y;
      return;
    }
  }
  snake.fx = 0;
  snake.fy = 0;
}
void snakeInit(bool wrap) {
  snake.n = 3;
  snake.x[0] = 4;
  snake.y[0] = 4;
  snake.x[1] = 3;
  snake.y[1] = 4;
  snake.x[2] = 2;
  snake.y[2] = 4;
  snake.dx = 1;
  snake.dy = 0;
  snake.ndx = 1;
  snake.ndy = 0;
  snake.wrap = wrap;
  snake.ms = 280;
  snake.last = millis();
  snakeFood();
}
void snakeUpdate(uint32_t now) {
  if (take(B_UP) && snake.dy != 1) {
    snake.ndx = 0;
    snake.ndy = -1;
    beepBtn();
  }
  if (take(B_DOWN) && snake.dy != -1) {
    snake.ndx = 0;
    snake.ndy = 1;
    beepBtn();
  }
  if (take(B_LEFT) && snake.dx != 1) {
    snake.ndx = -1;
    snake.ndy = 0;
    beepBtn();
  }
  if (take(B_RIGHT) && snake.dx != -1) {
    snake.ndx = 1;
    snake.ndy = 0;
    beepBtn();
  }
  if (now - snake.last < snake.ms) return;
  snake.last = now;
  snake.dx = snake.ndx;
  snake.dy = snake.ndy;
  int8_t nx = static_cast<int8_t>(snake.x[0] + snake.dx);
  int8_t ny = static_cast<int8_t>(snake.y[0] + snake.dy);
  if (snake.wrap) {
    if (nx < 0) nx = 7;
    if (nx > 7) nx = 0;
    if (ny < 0) ny = 7;
    if (ny > 7) ny = 0;
  } else if (nx < 0 || nx > 7 || ny < 0 || ny > 7) {
    beepHit();
    finish(false, "Hit wall");
    return;
  }
  if (snakeHas(nx, ny, snake.n)) {
    beepHit();
    finish(false, "Hit body");
    return;
  }

  bool eat = (nx == snake.fx && ny == snake.fy);
  if (eat && snake.n < 64) snake.n++;
  for (int8_t i = static_cast<int8_t>(snake.n - 1); i > 0; --i) {
    snake.x[i] = snake.x[i - 1];
    snake.y[i] = snake.y[i - 1];
  }
  snake.x[0] = nx;
  snake.y[0] = ny;

  if (eat) {
    score += 10;
    beepScore();
    if (snake.n == 64) {
      finish(true, "Board full");
      return;
    }
    snakeFood();
    if (snake.ms > 100) snake.ms = static_cast<uint16_t>(snake.ms - 10);
  }
}
void snakeDraw() {
  clearFrame();
  uint32_t headColor = snake.wrap ? C(150, 20, 150) : C(30, 130, 30);
  uint32_t bodyColor = snake.wrap ? C(90, 35, 90) : C(20, 70, 20);
  uint32_t foodColor = snake.wrap ? C(0, 85, 85) : C(85, 12, 12);
  px(snake.fx, snake.fy, foodColor);
  for (int8_t i = static_cast<int8_t>(snake.n - 1); i >= 0; --i) {
    px(snake.x[i], snake.y[i], i == 0 ? headColor : bodyColor);
  }
}

struct Tetris {
  uint8_t b[8][8];
  int8_t px;
  int8_t py;
  uint8_t t;
  uint8_t r;
  uint16_t ms;
  uint32_t last;
  uint32_t soft;
} tetris;

struct TRot {
  int8_t x[4];
  int8_t y[4];
};
const TRot TSH[5][4] = {
    {{{0, 1, 2, 3}, {1, 1, 1, 1}}, {{2, 2, 2, 2}, {0, 1, 2, 3}}, {{0, 1, 2, 3}, {2, 2, 2, 2}}, {{1, 1, 1, 1}, {0, 1, 2, 3}}},
    {{{1, 2, 1, 2}, {0, 0, 1, 1}}, {{1, 2, 1, 2}, {0, 0, 1, 1}}, {{1, 2, 1, 2}, {0, 0, 1, 1}}, {{1, 2, 1, 2}, {0, 0, 1, 1}}},
    {{{1, 0, 1, 2}, {0, 1, 1, 1}}, {{1, 1, 2, 1}, {0, 1, 1, 2}}, {{0, 1, 2, 1}, {1, 1, 1, 2}}, {{1, 0, 1, 1}, {0, 1, 1, 2}}},
    {{{0, 0, 0, 1}, {0, 1, 2, 2}}, {{0, 1, 2, 2}, {1, 1, 1, 0}}, {{0, 1, 1, 1}, {0, 0, 1, 2}}, {{0, 0, 1, 2}, {2, 1, 1, 1}}},
    {{{1, 2, 0, 1}, {0, 0, 1, 1}}, {{1, 1, 2, 2}, {0, 1, 1, 2}}, {{1, 2, 0, 1}, {1, 1, 2, 2}}, {{0, 0, 1, 1}, {0, 1, 1, 2}}}};

bool tCan(int8_t px0, int8_t py0, uint8_t t, uint8_t r) {
  for (uint8_t i = 0; i < 4; ++i) {
    int8_t x = static_cast<int8_t>(px0 + TSH[t][r].x[i]);
    int8_t y = static_cast<int8_t>(py0 + TSH[t][r].y[i]);
    if (x < 0 || x > 7 || y < 0 || y > 7) return false;
    if (tetris.b[y][x]) return false;
  }
  return true;
}
void tSpawn() {
  tetris.t = static_cast<uint8_t>(random(0, 5));
  tetris.r = 0;
  tetris.px = 2;
  tetris.py = 0;
  if (!tCan(tetris.px, tetris.py, tetris.t, tetris.r)) {
    beepHit();
    finish(false, "Stacked out");
  }
}
void tLock() {
  for (uint8_t i = 0; i < 4; ++i) {
    int8_t x = static_cast<int8_t>(tetris.px + TSH[tetris.t][tetris.r].x[i]);
    int8_t y = static_cast<int8_t>(tetris.py + TSH[tetris.t][tetris.r].y[i]);
    if (x >= 0 && x < 8 && y >= 0 && y < 8) tetris.b[y][x] = static_cast<uint8_t>(tetris.t + 1);
  }
}
void tClear() {
  uint8_t lines = 0;
  for (int8_t y = 7; y >= 0; --y) {
    bool full = true;
    for (uint8_t x = 0; x < 8; ++x)
      if (!tetris.b[y][x]) full = false;
    if (!full) continue;
    lines++;
    for (int8_t p = y; p > 0; --p)
      for (uint8_t x = 0; x < 8; ++x) tetris.b[p][x] = tetris.b[p - 1][x];
    for (uint8_t x = 0; x < 8; ++x) tetris.b[0][x] = 0;
    y++;
  }
  if (lines) {
    score += static_cast<int16_t>(lines * 10);
    beepScore();
  }
}
void tStep() {
  if (tCan(tetris.px, static_cast<int8_t>(tetris.py + 1), tetris.t, tetris.r)) {
    tetris.py++;
    return;
  }
  tLock();
  tClear();
  tSpawn();
}
void tInit() {
  memset(tetris.b, 0, sizeof(tetris.b));
  tetris.ms = 640;
  tetris.last = millis();
  tetris.soft = millis();
  tSpawn();
}
void tUpdate(uint32_t now) {
  if (take(B_LEFT) && tCan(static_cast<int8_t>(tetris.px - 1), tetris.py, tetris.t, tetris.r)) {
    tetris.px--;
    beepBtn();
  }
  if (take(B_RIGHT) && tCan(static_cast<int8_t>(tetris.px + 1), tetris.py, tetris.t, tetris.r)) {
    tetris.px++;
    beepBtn();
  }
  if (take(B_PAUSE)) {
    uint8_t nr = static_cast<uint8_t>((tetris.r + 1) % 4);
    if (tCan(tetris.px, tetris.py, tetris.t, nr)) {
      tetris.r = nr;
      beepBtn();
    }
  }
  if (take(B_DOWN) || (down(B_DOWN) && now - tetris.soft >= 90)) {
    tetris.soft = now;
    tStep();
  }
  if (now - tetris.last >= tetris.ms) {
    tetris.last = now;
    tStep();
  }
  int16_t m = static_cast<int16_t>(640 - score * 3);
  if (m < 120) m = 120;
  tetris.ms = static_cast<uint16_t>(m);
}
void tDraw() {
  clearFrame();
  const uint32_t col[6] = {0, 0x000022, 0x221100, 0x002200, 0x220000, 0x001122};
  for (uint8_t y = 0; y < 8; ++y)
    for (uint8_t x = 0; x < 8; ++x)
      if (tetris.b[y][x]) {
        uint32_t c = col[tetris.b[y][x]];
        px(x, y, C(static_cast<uint8_t>((c >> 16) & 0xFF), static_cast<uint8_t>((c >> 8) & 0xFF), static_cast<uint8_t>(c & 0xFF)));
      }
  for (uint8_t i = 0; i < 4; ++i) px(static_cast<int8_t>(tetris.px + TSH[tetris.t][tetris.r].x[i]), static_cast<int8_t>(tetris.py + TSH[tetris.t][tetris.r].y[i]), C(60, 60, 60));
}

struct Flappy {
  int16_t y8;
  int16_t v8;
  int8_t px0;
  int8_t gy;
  bool passed;
  uint16_t ms;
  uint32_t last;
} flappy;
void fPipe() {
  flappy.px0 = 8;
  flappy.gy = static_cast<int8_t>(random(1, 5));
  flappy.passed = false;
}
void fInit() {
  flappy.y8 = 28;
  flappy.v8 = 0;
  flappy.ms = 125;
  flappy.last = millis();
  fPipe();
}
void fUpdate(uint32_t now) {
  if (take(B_UP) || take(B_PAUSE)) {
    flappy.v8 = -14;
    beepBtn();
  }
  if (now - flappy.last < flappy.ms) return;
  flappy.last = now;
  flappy.v8 += 4;
  if (flappy.v8 > 18) flappy.v8 = 18;
  flappy.y8 = static_cast<int16_t>(flappy.y8 + flappy.v8);
  int8_t by = static_cast<int8_t>(flappy.y8 / 8);
  flappy.px0--;
  if (flappy.px0 < -2) fPipe();
  if (!flappy.passed && flappy.px0 + 1 < 2) {
    flappy.passed = true;
    score++;
    beepScore();
  }
  if (flappy.y8 < 0 || by > 7) {
    beepHit();
    finish(false, "Crashed");
    return;
  }
  if (2 >= flappy.px0 && 2 < flappy.px0 + 2) {
    if (by < flappy.gy || by >= flappy.gy + 3) {
      beepHit();
      finish(false, "Hit pipe");
      return;
    }
  }
  int16_t m = static_cast<int16_t>(125 - score * 2);
  if (m < 65) m = 65;
  flappy.ms = static_cast<uint16_t>(m);
}
void fDraw() {
  clearFrame();
  for (int8_t x = flappy.px0; x < flappy.px0 + 2; ++x)
    for (uint8_t y = 0; y < 8; ++y)
      if (y < flappy.gy || y >= flappy.gy + 3) px(x, y, C(0, 35, 8));
  px(2, static_cast<int8_t>(flappy.y8 / 8), C(70, 55, 0));
}

struct FlappyEasy {
  int8_t by;
  int8_t vv;
  int8_t px0;
  int8_t gapY;
  uint8_t gap;
  uint16_t ms;
  uint32_t last;
} flappyEasy;
void feInit() {
  flappyEasy.by = 4;
  flappyEasy.vv = 0;
  flappyEasy.px0 = 7;
  flappyEasy.gapY = static_cast<int8_t>(random(1, 5));
  flappyEasy.gap = 3;
  flappyEasy.ms = 200;
  flappyEasy.last = millis();
}
void feUpdate(uint32_t now) {
  if (take(B_UP) || take(B_PAUSE)) {
    flappyEasy.vv = -2;
    beepBtn();
  }
  if (now - flappyEasy.last < flappyEasy.ms) return;
  flappyEasy.last = now;

  flappyEasy.vv++;
  flappyEasy.by = static_cast<int8_t>(flappyEasy.by + flappyEasy.vv);
  if (flappyEasy.by < 0 || flappyEasy.by >= 8) {
    beepHit();
    finish(false, "Crashed");
    return;
  }

  flappyEasy.px0--;
  if (flappyEasy.px0 < 0) {
    flappyEasy.px0 = 7;
    flappyEasy.gapY = static_cast<int8_t>(random(1, 5));
    score += 10;
    beepScore();
  }

  if (flappyEasy.px0 == 0) {
    if (flappyEasy.by < flappyEasy.gapY || flappyEasy.by >= flappyEasy.gapY + flappyEasy.gap) {
      beepHit();
      finish(false, "Hit pipe");
      return;
    }
  }
}
void feDraw() {
  clearFrame();
  px(0, flappyEasy.by, C(70, 55, 0));
  for (uint8_t y = 0; y < 8; ++y) {
    if (y < flappyEasy.gapY || y >= flappyEasy.gapY + flappyEasy.gap) {
      px(flappyEasy.px0, static_cast<int8_t>(y), C(0, 55, 0));
    }
  }
}

struct AstRock {
  int8_t x;
  int8_t y;
  bool on;
  uint16_t ms;
  uint32_t last;
};
struct Ast {
  int8_t sx;
  Bullet b[5];
  AstRock r[8];
  uint16_t sp;
  uint32_t lMove;
  uint32_t lBul;
  uint32_t lSpawn;
  uint32_t lShot;
} ast;
void aInit() {
  ast.sx = 3;
  ast.sp = 900;
  ast.lMove = ast.lBul = ast.lSpawn = ast.lShot = millis();
  for (uint8_t i = 0; i < 5; ++i) ast.b[i].on = false;
  for (uint8_t i = 0; i < 8; ++i) ast.r[i].on = false;
}
void aShot() {
  for (uint8_t i = 0; i < 5; ++i)
    if (!ast.b[i].on) {
      ast.b[i] = {ast.sx, 6, true};
      return;
    }
}
void aSpawn() {
  for (uint8_t i = 0; i < 8; ++i)
    if (!ast.r[i].on) {
      int16_t m = static_cast<int16_t>(random(180, 320) - score * 2);
      if (m < 90) m = 90;
      ast.r[i] = {static_cast<int8_t>(random(0, 8)), 0, true, static_cast<uint16_t>(m), millis()};
      return;
    }
}
void aUpdate(uint32_t now) {
  if (down(B_LEFT) && now - ast.lMove >= 90) {
    ast.lMove = now;
    if (ast.sx > 0) {
      ast.sx--;
      beepBtn();
    }
  }
  if (down(B_RIGHT) && now - ast.lMove >= 90) {
    ast.lMove = now;
    if (ast.sx < 7) {
      ast.sx++;
      beepBtn();
    }
  }
  if ((take(B_UP) || take(B_PAUSE)) && now - ast.lShot >= 170) {
    ast.lShot = now;
    aShot();
    beepBtn();
  }
  if (now - ast.lBul >= 85) {
    ast.lBul = now;
    for (uint8_t i = 0; i < 5; ++i)
      if (ast.b[i].on) {
        ast.b[i].y--;
        if (ast.b[i].y < 0) ast.b[i].on = false;
      }
  }
  if (now - ast.lSpawn >= ast.sp) {
    ast.lSpawn = now;
    aSpawn();
  }
  for (uint8_t i = 0; i < 8; ++i)
    if (ast.r[i].on && now - ast.r[i].last >= ast.r[i].ms) {
      ast.r[i].last = now;
      ast.r[i].y++;
      if (ast.r[i].y > 7) ast.r[i].on = false;
    }
  for (uint8_t bi = 0; bi < 5; ++bi)
    if (ast.b[bi].on)
      for (uint8_t ri = 0; ri < 8; ++ri)
        if (ast.r[ri].on && ast.b[bi].x == ast.r[ri].x && ast.b[bi].y == ast.r[ri].y) {
          ast.b[bi].on = false;
          ast.r[ri].on = false;
          score++;
          beepScore();
          break;
        }
  for (uint8_t i = 0; i < 8; ++i)
    if (ast.r[i].on && ast.r[i].x == ast.sx && ast.r[i].y == 7) {
      beepHit();
      finish(false, "Ship hit");
      return;
    }
  int16_t m = static_cast<int16_t>(900 - score * 16);
  if (m < 220) m = 220;
  ast.sp = static_cast<uint16_t>(m);
}
void aDraw() {
  clearFrame();
  for (uint8_t i = 0; i < 8; ++i)
    if (ast.r[i].on) px(ast.r[i].x, ast.r[i].y, C(40, 18, 0));
  for (uint8_t i = 0; i < 5; ++i)
    if (ast.b[i].on) px(ast.b[i].x, ast.b[i].y, C(65, 65, 65));
  px(ast.sx, 7, C(0, 40, 45));
}

struct AstEasyBullet {
  int8_t x;
  int8_t y;
  int8_t dx;
  int8_t dy;
  bool on;
};
struct AstEasyRock {
  int8_t x;
  int8_t y;
  bool on;
};
struct AsteroidsEasy {
  int8_t sx;
  int8_t sy;
  AstEasyBullet b[3];
  AstEasyRock r[5];
  uint32_t lMove;
  uint32_t lSpawn;
  uint32_t lBul;
  uint32_t lInput;
  uint32_t lShot;
} astEasy;
void aeInit() {
  astEasy.sx = 4;
  astEasy.sy = 7;
  astEasy.lMove = astEasy.lSpawn = astEasy.lBul = astEasy.lInput = astEasy.lShot = millis();
  for (uint8_t i = 0; i < 3; ++i) astEasy.b[i].on = false;
  for (uint8_t i = 0; i < 5; ++i) astEasy.r[i].on = false;
}
void aeShoot() {
  for (uint8_t i = 0; i < 3; ++i) {
    if (astEasy.b[i].on) continue;
    astEasy.b[i] = {astEasy.sx, astEasy.sy, 0, -1, true};
    return;
  }
}
void aeUpdate(uint32_t now) {
  if (down(B_LEFT) && now - astEasy.lInput >= 100) {
    astEasy.lInput = now;
    if (astEasy.sx > 0) {
      astEasy.sx--;
      beepBtn();
    }
  }
  if (down(B_RIGHT) && now - astEasy.lInput >= 100) {
    astEasy.lInput = now;
    if (astEasy.sx < 7) {
      astEasy.sx++;
      beepBtn();
    }
  }
  if (take(B_PAUSE) && now - astEasy.lShot >= 150) {
    astEasy.lShot = now;
    aeShoot();
    beepBtn();
  }

  if (now - astEasy.lSpawn > 2000) {
    astEasy.lSpawn = now;
    for (uint8_t i = 0; i < 5; ++i) {
      if (astEasy.r[i].on) continue;
      astEasy.r[i] = {static_cast<int8_t>(random(0, 8)), 0, true};
      break;
    }
  }

  if (now - astEasy.lMove > 300) {
    astEasy.lMove = now;
    for (uint8_t i = 0; i < 5; ++i) {
      if (!astEasy.r[i].on) continue;
      astEasy.r[i].y++;
      if (astEasy.r[i].y >= 8) {
        astEasy.r[i].on = false;
      } else if (astEasy.r[i].x == astEasy.sx && astEasy.r[i].y == astEasy.sy) {
        beepHit();
        finish(false, "Ship hit");
        return;
      }
    }
  }

  if (now - astEasy.lBul > 90) {
    astEasy.lBul = now;
    for (uint8_t i = 0; i < 3; ++i) {
      if (!astEasy.b[i].on) continue;
      astEasy.b[i].x = static_cast<int8_t>(astEasy.b[i].x + astEasy.b[i].dx);
      astEasy.b[i].y = static_cast<int8_t>(astEasy.b[i].y + astEasy.b[i].dy);
      if (astEasy.b[i].x < 0 || astEasy.b[i].x >= 8 || astEasy.b[i].y < 0 || astEasy.b[i].y >= 8) {
        astEasy.b[i].on = false;
        continue;
      }
      for (uint8_t j = 0; j < 5; ++j) {
        if (!astEasy.r[j].on) continue;
        if (astEasy.r[j].x == astEasy.b[i].x && astEasy.r[j].y == astEasy.b[i].y) {
          astEasy.r[j].on = false;
          astEasy.b[i].on = false;
          score += 10;
          beepScore();
          break;
        }
      }
    }
  }
}
void aeDraw() {
  clearFrame();
  px(astEasy.sx, astEasy.sy, C(0, 55, 0));
  for (uint8_t i = 0; i < 5; ++i)
    if (astEasy.r[i].on) px(astEasy.r[i].x, astEasy.r[i].y, C(65, 0, 0));
  for (uint8_t i = 0; i < 3; ++i)
    if (astEasy.b[i].on) px(astEasy.b[i].x, astEasy.b[i].y, C(70, 55, 0));
}

struct PacEasy {
  int8_t px0;
  int8_t py0;
  int8_t dir;
  int8_t gx;
  int8_t gy;
  uint8_t pellets[8][8];
  int16_t left;
  uint32_t lPac;
  uint32_t lGhost;
} pacEasy;
void peFill() {
  pacEasy.left = 0;
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      pacEasy.pellets[y][x] = 1;
      pacEasy.left++;
    }
  }
  if (pacEasy.pellets[pacEasy.py0][pacEasy.px0]) {
    pacEasy.pellets[pacEasy.py0][pacEasy.px0] = 0;
    pacEasy.left--;
  }
}
void peInit() {
  pacEasy.px0 = 4;
  pacEasy.py0 = 4;
  pacEasy.dir = 0;
  pacEasy.gx = 0;
  pacEasy.gy = 0;
  pacEasy.lPac = millis();
  pacEasy.lGhost = millis();
  peFill();
}
void peUpdate(uint32_t now) {
  if (take(B_UP))
    pacEasy.dir = 3;
  else if (take(B_DOWN))
    pacEasy.dir = 1;
  else if (take(B_LEFT))
    pacEasy.dir = 2;
  else if (take(B_RIGHT))
    pacEasy.dir = 0;

  if (now - pacEasy.lPac > 250) {
    pacEasy.lPac = now;
    int8_t nx = pacEasy.px0;
    int8_t ny = pacEasy.py0;
    switch (pacEasy.dir) {
      case 0:
        nx++;
        break;
      case 1:
        ny++;
        break;
      case 2:
        nx--;
        break;
      default:
        ny--;
        break;
    }
    if (nx < 0) nx = 7;
    if (nx >= 8) nx = 0;
    if (ny < 0) ny = 7;
    if (ny >= 8) ny = 0;
    pacEasy.px0 = nx;
    pacEasy.py0 = ny;

    if (pacEasy.pellets[pacEasy.py0][pacEasy.px0]) {
      pacEasy.pellets[pacEasy.py0][pacEasy.px0] = 0;
      pacEasy.left--;
      score += 10;
      beepScore();
      if (pacEasy.left == 0) {
        score += 100;
        peFill();
      }
    }
  }

  if (now - pacEasy.lGhost > 400) {
    pacEasy.lGhost = now;
    if (abs(pacEasy.gx - pacEasy.px0) > abs(pacEasy.gy - pacEasy.py0)) {
      pacEasy.gx += (pacEasy.gx < pacEasy.px0) ? 1 : -1;
    } else {
      pacEasy.gy += (pacEasy.gy < pacEasy.py0) ? 1 : -1;
    }
    if (pacEasy.gx < 0) pacEasy.gx = 7;
    if (pacEasy.gx >= 8) pacEasy.gx = 0;
    if (pacEasy.gy < 0) pacEasy.gy = 7;
    if (pacEasy.gy >= 8) pacEasy.gy = 0;
  }

  if (pacEasy.px0 == pacEasy.gx && pacEasy.py0 == pacEasy.gy) {
    beepHit();
    finish(false, "Caught");
  }
}
void peDraw() {
  clearFrame();
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      if (pacEasy.pellets[y][x]) px(static_cast<int8_t>(x), static_cast<int8_t>(y), C(0, 28, 45));
    }
  }
  px(pacEasy.px0, pacEasy.py0, C(130, 110, 0));
  px(pacEasy.gx, pacEasy.gy, C(65, 0, 0));
}

const uint8_t PAC_WORLDS[4][8][8] = {
    {
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 1, 1, 1, 1, 1, 1, 0},
        {0, 1, 0, 0, 1, 0, 1, 0},
        {0, 1, 1, 1, 1, 0, 1, 0},
        {0, 1, 0, 1, 1, 1, 1, 0},
        {0, 1, 1, 0, 0, 0, 1, 0},
        {0, 1, 1, 1, 1, 1, 1, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
    },
    {
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 1, 1, 1, 1, 1, 1, 0},
        {0, 1, 0, 1, 0, 1, 1, 0},
        {0, 1, 1, 1, 0, 1, 1, 0},
        {0, 1, 0, 1, 1, 1, 0, 0},
        {0, 1, 1, 0, 1, 1, 1, 0},
        {0, 1, 1, 1, 1, 0, 1, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
    },
    {
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 1, 1, 1, 0, 1, 1, 0},
        {0, 1, 0, 1, 1, 1, 0, 0},
        {0, 1, 1, 0, 1, 0, 1, 0},
        {0, 0, 1, 1, 1, 1, 1, 0},
        {0, 1, 0, 1, 0, 1, 1, 0},
        {0, 1, 1, 1, 1, 1, 1, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
    },
    {
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 1, 1, 1, 1, 1, 1, 0},
        {0, 1, 0, 0, 1, 0, 1, 0},
        {0, 1, 1, 1, 1, 1, 1, 0},
        {0, 1, 0, 1, 0, 1, 0, 0},
        {0, 1, 1, 1, 1, 1, 1, 0},
        {0, 1, 1, 0, 1, 0, 1, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
    },
};
struct Pac {
  uint8_t c[8][8];
  int8_t px0;
  int8_t py0;
  int8_t gx;
  int8_t gy;
  int8_t dx;
  int8_t dy;
  int8_t ndx;
  int8_t ndy;
  uint16_t pellets;
  uint16_t pMs;
  uint16_t gMs;
  uint32_t lp;
  uint32_t lg;
  uint8_t world;
} pac;
bool pWall(int8_t x, int8_t y) {
  if (x < 0 || x > 7 || y < 0 || y > 7) return true;
  return pac.c[y][x] == 0;
}
void pLoad() {
  const uint8_t (*map)[8] = PAC_WORLDS[pac.world];
  pac.pellets = 0;
  for (uint8_t y = 0; y < 8; ++y)
    for (uint8_t x = 0; x < 8; ++x)
      if (map[y][x] == 0)
        pac.c[y][x] = 0;
      else {
        pac.c[y][x] = 1;
        pac.pellets++;
      }
  if (pac.c[pac.py0][pac.px0] == 1) {
    pac.c[pac.py0][pac.px0] = 2;
    pac.pellets--;
  }
  if (pac.c[pac.gy][pac.gx] == 1) {
    pac.c[pac.gy][pac.gx] = 2;
    pac.pellets--;
  }
}
void pInit() {
  pac.world = 0;
  pac.px0 = 1;
  pac.py0 = 1;
  pac.gx = 6;
  pac.gy = 6;
  pac.dx = 1;
  pac.dy = 0;
  pac.ndx = 1;
  pac.ndy = 0;
  pac.pMs = 210;
  pac.gMs = 360;
  pac.lp = pac.lg = millis();
  pLoad();
}
void pRespawn() {
  const uint8_t (*map)[8] = PAC_WORLDS[pac.world];
  for (uint8_t y = 0; y < 8; ++y)
    for (uint8_t x = 0; x < 8; ++x)
      if (map[y][x] != 0) pac.c[y][x] = 1;
  pac.c[pac.py0][pac.px0] = 2;
  pac.c[pac.gy][pac.gx] = 2;
  pac.pellets = 0;
  for (uint8_t y = 0; y < 8; ++y)
    for (uint8_t x = 0; x < 8; ++x)
      if (pac.c[y][x] == 1) pac.pellets++;
}
void pGhost() {
  const int8_t d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  int8_t bx = pac.gx, by = pac.gy, bd = 127;
  for (uint8_t i = 0; i < 4; ++i) {
    int8_t nx = static_cast<int8_t>(pac.gx + d[i][0]), ny = static_cast<int8_t>(pac.gy + d[i][1]);
    if (pWall(nx, ny)) continue;
    int8_t md = static_cast<int8_t>(abs(nx - pac.px0) + abs(ny - pac.py0));
    if (md < bd || (md == bd && random(0, 2) == 0)) {
      bd = md;
      bx = nx;
      by = ny;
    }
  }
  pac.gx = bx;
  pac.gy = by;
}
void pUpdate(uint32_t now) {
  if (take(B_UP)) {
    pac.ndx = 0;
    pac.ndy = -1;
    beepBtn();
  }
  if (take(B_DOWN)) {
    pac.ndx = 0;
    pac.ndy = 1;
    beepBtn();
  }
  if (take(B_LEFT)) {
    pac.ndx = -1;
    pac.ndy = 0;
    beepBtn();
  }
  if (take(B_RIGHT)) {
    pac.ndx = 1;
    pac.ndy = 0;
    beepBtn();
  }
  if (now - pac.lp >= pac.pMs) {
    pac.lp = now;
    if (!pWall(static_cast<int8_t>(pac.px0 + pac.ndx), static_cast<int8_t>(pac.py0 + pac.ndy))) {
      pac.dx = pac.ndx;
      pac.dy = pac.ndy;
    }
    int8_t nx = static_cast<int8_t>(pac.px0 + pac.dx), ny = static_cast<int8_t>(pac.py0 + pac.dy);
    if (!pWall(nx, ny)) {
      pac.px0 = nx;
      pac.py0 = ny;
    }
    if (pac.c[pac.py0][pac.px0] == 1) {
      pac.c[pac.py0][pac.px0] = 2;
      if (pac.pellets) pac.pellets--;
      score++;
      beepScore();
    }
  }
  if (now - pac.lg >= pac.gMs) {
    pac.lg = now;
    pGhost();
  }
  if (pac.px0 == pac.gx && pac.py0 == pac.gy) {
    beepHit();
    finish(false, "Caught");
    return;
  }
  if (pac.pellets == 0) {
    if (pac.world < 3) {
      pac.world++;
      score += 25;
      beepScore();
      pac.px0 = 1;
      pac.py0 = 1;
      pac.gx = 6;
      pac.gy = 6;
      pac.dx = 1;
      pac.dy = 0;
      pac.ndx = 1;
      pac.ndy = 0;
      if (pac.gMs > 120) pac.gMs = static_cast<uint16_t>(pac.gMs - 18);
      if (pac.pMs > 145) pac.pMs = static_cast<uint16_t>(pac.pMs - 10);
      pRespawn();
    } else {
      score += 100;
      finish(true, "World 4 clear");
      return;
    }
  }
}
void pDraw() {
  clearFrame();
  for (uint8_t y = 0; y < 8; ++y)
    for (uint8_t x = 0; x < 8; ++x) {
      if (pac.c[y][x] == 0)
        px(x, y, C(0, 0, 24));
      else if (pac.c[y][x] == 1)
        px(x, y, C(170, 150, 55));
    }
  px(pac.gx, pac.gy, C(45, 0, 0));
  px(pac.px0, pac.py0, C(255, 95, 0));
}

struct Enemy {
  int8_t x;
  int8_t y;
  bool on;
};
struct Shooter {
  int8_t sx;
  Bullet pb[6];
  Bullet eb[5];
  Enemy e[8];
  uint16_t eMs;
  uint16_t spMs;
  uint32_t lMove;
  uint32_t lPB;
  uint32_t lEB;
  uint32_t lES;
  uint32_t lSpawn;
  uint32_t lShot;
  uint32_t lEShoot;
} sh;
void shInit() {
  sh.sx = 3;
  sh.eMs = 650;
  sh.spMs = 1200;
  sh.lMove = sh.lPB = sh.lEB = sh.lES = sh.lSpawn = sh.lShot = sh.lEShoot = millis();
  for (uint8_t i = 0; i < 6; ++i) sh.pb[i].on = false;
  for (uint8_t i = 0; i < 5; ++i) sh.eb[i].on = false;
  for (uint8_t i = 0; i < 8; ++i) sh.e[i].on = false;
}
void shFire() {
  for (uint8_t i = 0; i < 6; ++i)
    if (!sh.pb[i].on) {
      sh.pb[i] = {sh.sx, 6, true};
      return;
    }
}
void shSpawn() {
  for (uint8_t i = 0; i < 8; ++i)
    if (!sh.e[i].on) {
      sh.e[i] = {static_cast<int8_t>(random(0, 8)), 0, true};
      return;
    }
}
void shEFire(int8_t x, int8_t y) {
  for (uint8_t i = 0; i < 5; ++i)
    if (!sh.eb[i].on) {
      sh.eb[i] = {x, y, true};
      return;
    }
}
void shUpdate(uint32_t now) {
  if (down(B_LEFT) && now - sh.lMove >= 110) {
    sh.lMove = now;
    if (sh.sx > 0) {
      sh.sx--;
      beepBtn();
    }
  }
  if (down(B_RIGHT) && now - sh.lMove >= 110) {
    sh.lMove = now;
    if (sh.sx < 7) {
      sh.sx++;
      beepBtn();
    }
  }
  if ((take(B_UP) || take(B_PAUSE)) && now - sh.lShot >= 280) {
    sh.lShot = now;
    shFire();
    beepBtn();
  }
  if (now - sh.lSpawn >= sh.spMs) {
    sh.lSpawn = now;
    shSpawn();
  }
  if (now - sh.lPB >= 115) {
    sh.lPB = now;
    for (uint8_t i = 0; i < 6; ++i)
      if (sh.pb[i].on) {
        sh.pb[i].y--;
        if (sh.pb[i].y < 0) sh.pb[i].on = false;
      }
  }
  if (now - sh.lES >= sh.eMs) {
    sh.lES = now;
    for (uint8_t i = 0; i < 8; ++i)
      if (sh.e[i].on) {
        if (random(0, 3) == 0) {
          int8_t nx = static_cast<int8_t>(sh.e[i].x + random(-1, 2));
          if (nx >= 0 && nx < 8) sh.e[i].x = nx;
        }
        sh.e[i].y++;
        if (sh.e[i].y >= 7) {
          beepHit();
          finish(false, "Line broken");
          return;
        }
      }
  }
  if (now - sh.lEShoot >= 760) {
    sh.lEShoot = now;
    uint8_t s = static_cast<uint8_t>(random(0, 8));
    for (uint8_t j = 0; j < 8; ++j) {
      uint8_t i = static_cast<uint8_t>((s + j) % 8);
      if (sh.e[i].on) {
        shEFire(sh.e[i].x, static_cast<int8_t>(sh.e[i].y + 1));
        break;
      }
    }
  }
  if (now - sh.lEB >= 170) {
    sh.lEB = now;
    for (uint8_t i = 0; i < 5; ++i)
      if (sh.eb[i].on) {
        sh.eb[i].y++;
        if (sh.eb[i].y > 7)
          sh.eb[i].on = false;
        else if (sh.eb[i].y == 7 && sh.eb[i].x == sh.sx) {
          beepHit();
          finish(false, "Shot down");
          return;
        }
      }
  }
  for (uint8_t bi = 0; bi < 6; ++bi)
    if (sh.pb[bi].on)
      for (uint8_t ei = 0; ei < 8; ++ei)
        if (sh.e[ei].on && sh.pb[bi].x == sh.e[ei].x && sh.pb[bi].y == sh.e[ei].y) {
          sh.pb[bi].on = false;
          sh.e[ei].on = false;
          score++;
          beepScore();
          break;
        }
  int16_t em = static_cast<int16_t>(650 - score * 2);
  if (em < 260) em = 260;
  sh.eMs = static_cast<uint16_t>(em);
  int16_t sm = static_cast<int16_t>(1200 - score * 5);
  if (sm < 420) sm = 420;
  sh.spMs = static_cast<uint16_t>(sm);
}
void shDraw() {
  clearFrame();
  for (uint8_t i = 0; i < 8; ++i)
    if (sh.e[i].on) px(sh.e[i].x, sh.e[i].y, C(46, 0, 20));
  for (uint8_t i = 0; i < 6; ++i)
    if (sh.pb[i].on) px(sh.pb[i].x, sh.pb[i].y, C(65, 65, 65));
  for (uint8_t i = 0; i < 5; ++i)
    if (sh.eb[i].on) px(sh.eb[i].x, sh.eb[i].y, C(55, 10, 0));
  px(sh.sx, 7, C(0, 60, 10));
}

const uint8_t GLYPH_N[8] = {
    0b10000001, 0b11000001, 0b10100001, 0b10010001,
    0b10001001, 0b10000101, 0b10000011, 0b10000001,
};
const uint8_t GLYPH_B[8] = {
    0b11111100, 0b10000010, 0b10000010, 0b11111100,
    0b10000010, 0b10000010, 0b11111100, 0b00000000,
};
const uint8_t GLYPH_6[8] = {
    0b00111110, 0b01000000, 0b10000000, 0b11111100,
    0b10000010, 0b10000010, 0b01111100, 0b00000000,
};
const uint8_t GLYPH_HEART[8] = {
    0b00000000, 0b01100110, 0b11111111, 0b11111111,
    0b11111111, 0b01111110, 0b00111100, 0b00011000,
};
void drawGlyph(const uint8_t rows[8], uint32_t color) {
  clearFrame();
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      if (rows[y] & static_cast<uint8_t>(1 << x)) {
        px(static_cast<int8_t>(x), static_cast<int8_t>(y), color);
      }
    }
  }
  showFrame();
}
void playBootMatrixSequence() {
  constexpr uint16_t frameMs = 1333;  // 0.75 FPS
  drawGlyph(GLYPH_N, C(0, 80, 255));
  delay(frameMs);
  drawGlyph(GLYPH_B, C(255, 85, 0));
  delay(frameMs);
  drawGlyph(GLYPH_6, C(80, 255, 80));
  delay(frameMs);

  const uint8_t levels[8] = {30, 60, 95, 140, 200, 140, 95, 60};
  uint32_t start = millis();
  uint8_t i = 0;
  while (millis() - start < 4000) {
    uint8_t v = levels[i % 8];
    drawGlyph(GLYPH_HEART, C(v, 0, 0));
    delay(125);
    ++i;
  }
  clearFrame();
  showFrame();
}

struct Breakout {
  bool b[3][8];
  int8_t px0;
  int8_t bx;
  int8_t by;
  int8_t vx;
  int8_t vy;
  uint16_t ms;
  uint32_t last;
  uint32_t lMove;
} br;
void brBricks() {
  for (uint8_t y = 0; y < 3; ++y)
    for (uint8_t x = 0; x < 8; ++x) br.b[y][x] = true;
}
void brBall() {
  br.bx = 3;
  br.by = 6;
  br.vx = (random(0, 2) == 0) ? -1 : 1;
  br.vy = -1;
}
bool brAny() {
  for (uint8_t y = 0; y < 3; ++y)
    for (uint8_t x = 0; x < 8; ++x)
      if (br.b[y][x]) return true;
  return false;
}
void brInit() {
  brBricks();
  br.px0 = 2;
  brBall();
  br.ms = 200;
  br.last = br.lMove = millis();
}
void brUpdate(uint32_t now) {
  if (down(B_LEFT) && now - br.lMove >= 80) {
    br.lMove = now;
    if (br.px0 > 0) {
      br.px0--;
      beepBtn();
    }
  }
  if (down(B_RIGHT) && now - br.lMove >= 80) {
    br.lMove = now;
    if (br.px0 < 5) {
      br.px0++;
      beepBtn();
    }
  }
  if (now - br.last < br.ms) return;
  br.last = now;
  int8_t nx = static_cast<int8_t>(br.bx + br.vx), ny = static_cast<int8_t>(br.by + br.vy);
  if (nx < 0 || nx > 7) {
    br.vx = static_cast<int8_t>(-br.vx);
    nx = static_cast<int8_t>(br.bx + br.vx);
  }
  if (ny < 0) {
    br.vy = 1;
    ny = static_cast<int8_t>(br.by + br.vy);
  }
  if (ny >= 0 && ny < 3 && br.b[ny][nx]) {
    br.b[ny][nx] = false;
    br.vy = static_cast<int8_t>(-br.vy);
    ny = static_cast<int8_t>(br.by + br.vy);
    score++;
    beepScore();
  }
  if (ny >= 7) {
    bool topHit = (nx >= br.px0 && nx <= br.px0 + 2);
    bool leftSideHit = (nx == br.px0 - 1 && br.vx > 0);
    bool rightSideHit = (nx == br.px0 + 3 && br.vx < 0);
    if (topHit || leftSideHit || rightSideHit) {
      br.vy = -1;
      if (leftSideHit || rightSideHit) {
        br.vx = static_cast<int8_t>(-br.vx);
      } else {
        int8_t h = static_cast<int8_t>(nx - (br.px0 + 1));
        if (h < 0) br.vx = -1;
        if (h > 0) br.vx = 1;
      }
      nx = static_cast<int8_t>(constrain(nx, 0, 7));
      ny = 6;
      beepBtn();
    } else {
      beepHit();
      finish(false, "Missed ball");
      return;
    }
  }
  br.bx = nx;
  br.by = ny;
  if (!brAny()) {
    score += 5;
    beepWin();
    brBricks();
    brBall();
    if (br.ms > 80) br.ms = static_cast<uint16_t>(br.ms - 12);
  }
}
void brDraw() {
  clearFrame();
  for (uint8_t y = 0; y < 3; ++y)
    for (uint8_t x = 0; x < 8; ++x)
      if (br.b[y][x]) px(x, y, C(static_cast<uint8_t>(35 + y * 6), static_cast<uint8_t>(10 + y * 4), 0));
  for (uint8_t x = 0; x < 3; ++x) px(static_cast<int8_t>(br.px0 + x), 7, C(0, 48, 50));
  px(br.bx, br.by, C(65, 65, 65));
}

struct TTT {
  uint8_t b[3][3];
  uint8_t cx;
  uint8_t cy;
  uint8_t turn;
  bool ai;
  bool waitAI;
  uint32_t aiAt;
} ttt;
void tttInit(bool ai) {
  memset(ttt.b, 0, sizeof(ttt.b));
  ttt.cx = 1;
  ttt.cy = 1;
  ttt.turn = 1;
  ttt.ai = ai;
  ttt.waitAI = false;
  ttt.aiAt = 0;
}
uint8_t tttRes() {
  for (uint8_t i = 0; i < 3; ++i) {
    if (ttt.b[i][0] && ttt.b[i][0] == ttt.b[i][1] && ttt.b[i][1] == ttt.b[i][2]) return ttt.b[i][0];
    if (ttt.b[0][i] && ttt.b[0][i] == ttt.b[1][i] && ttt.b[1][i] == ttt.b[2][i]) return ttt.b[0][i];
  }
  if (ttt.b[0][0] && ttt.b[0][0] == ttt.b[1][1] && ttt.b[1][1] == ttt.b[2][2]) return ttt.b[0][0];
  if (ttt.b[0][2] && ttt.b[0][2] == ttt.b[1][1] && ttt.b[1][1] == ttt.b[2][0]) return ttt.b[0][2];
  for (uint8_t y = 0; y < 3; ++y)
    for (uint8_t x = 0; x < 3; ++x)
      if (!ttt.b[y][x]) return 0;
  return 3;
}
bool tttFind(uint8_t m, uint8_t &ox, uint8_t &oy) {
  for (uint8_t y = 0; y < 3; ++y)
    for (uint8_t x = 0; x < 3; ++x)
      if (!ttt.b[y][x]) {
        ttt.b[y][x] = m;
        bool ok = tttRes() == m;
        ttt.b[y][x] = 0;
        if (ok) {
          ox = x;
          oy = y;
          return true;
        }
      }
  return false;
}
void tttEnd() {
  uint8_t r = tttRes();
  if (!r) return;
  if (r == 1) {
    score += 5;
    finish(true, ttt.ai ? "You win" : "X wins");
  } else if (r == 2) {
    finish(ttt.ai ? false : true, ttt.ai ? "AI wins" : "O wins");
  } else {
    finish(false, "Draw");
  }
}
void tttAI() {
  uint8_t x = 0, y = 0;
  if (tttFind(2, x, y) || tttFind(1, x, y)) {
    ttt.b[y][x] = 2;
    return;
  }
  if (!ttt.b[1][1]) {
    ttt.b[1][1] = 2;
    return;
  }
  const uint8_t p[9][2] = {{0, 0}, {2, 0}, {0, 2}, {2, 2}, {1, 0}, {0, 1}, {2, 1}, {1, 2}, {1, 1}};
  for (uint8_t i = 0; i < 9; ++i) {
    uint8_t px0 = p[i][0], py0 = p[i][1];
    if (!ttt.b[py0][px0]) {
      ttt.b[py0][px0] = 2;
      return;
    }
  }
}
void tttUpdate(uint32_t now) {
  if (ttt.ai && ttt.waitAI && now >= ttt.aiAt) {
    ttt.waitAI = false;
    tttAI();
    tttEnd();
    if (systemState == STATE_PLAYING) ttt.turn = 1;
    return;
  }
  if (ttt.ai && ttt.turn == 2) return;
  if (take(B_UP) && ttt.cy > 0) {
    ttt.cy--;
    beepBtn();
  }
  if (take(B_DOWN) && ttt.cy < 2) {
    ttt.cy++;
    beepBtn();
  }
  if (take(B_LEFT) && ttt.cx > 0) {
    ttt.cx--;
    beepBtn();
  }
  if (take(B_RIGHT) && ttt.cx < 2) {
    ttt.cx++;
    beepBtn();
  }
  if (take(B_PAUSE)) {
    if (ttt.b[ttt.cy][ttt.cx]) {
      beepHit();
      return;
    }
    ttt.b[ttt.cy][ttt.cx] = ttt.turn;
    score++;
    beepBtn();
    tttEnd();
    if (systemState != STATE_PLAYING) return;
    if (ttt.ai) {
      ttt.turn = 2;
      ttt.waitAI = true;
      ttt.aiAt = now + 230;
    } else {
      ttt.turn = (ttt.turn == 1) ? 2 : 1;
    }
  }
}
void tttDraw() {
  clearFrame();
  const int8_t cellX[3] = {1, 3, 6};
  const int8_t cellY[3] = {1, 3, 6};
  for (uint8_t i = 0; i < 8; ++i) {
    px(2, i, C(8, 8, 8));
    px(5, i, C(8, 8, 8));
    px(i, 2, C(8, 8, 8));
    px(i, 5, C(8, 8, 8));
  }
  for (uint8_t y = 0; y < 3; ++y)
    for (uint8_t x = 0; x < 3; ++x) {
      int8_t gx = cellX[x];
      int8_t gy = cellY[y];
      if (ttt.b[y][x] == 1)
        px(gx, gy, C(0, 52, 0));
      else if (ttt.b[y][x] == 2)
        px(gx, gy, C(55, 0, 0));
      else
        px(gx, gy, C(10, 10, 10));
      if (x == ttt.cx && y == ttt.cy && (!ttt.ai || ttt.turn == 1)) px(gx, gy, ttt.b[y][x] ? C(65, 65, 65) : C(0, 0, 65));
    }
}

struct Pong {
  int8_t p1x;
  int8_t p2x;
  int8_t bx;
  int8_t by;
  int8_t dx;
  int8_t dy;
  uint8_t pw;
  uint16_t ms;
  uint32_t last;
  uint32_t lMove;
  uint32_t freezeUntil;
} pong;
void pongInit() {
  pong.p1x = 3;
  pong.p2x = 3;
  pong.bx = 4;
  pong.by = 4;
  pong.dx = (random(0, 2) == 0) ? -1 : 1;
  pong.dy = (random(0, 2) == 0) ? -1 : 1;
  pong.pw = 2;
  pong.ms = 200;
  pong.last = millis();
  pong.lMove = millis();
  pong.freezeUntil = 0;
  score = 0;
}
void pongUpdate(uint32_t now) {
  if (down(B_LEFT) && now - pong.lMove >= 85) {
    pong.lMove = now;
    if (pong.p1x > 0) {
      pong.p1x--;
      beepBtn();
    }
  }
  if (down(B_RIGHT) && now - pong.lMove >= 85) {
    pong.lMove = now;
    if (pong.p1x < static_cast<int8_t>(8 - pong.pw)) {
      pong.p1x++;
      beepBtn();
    }
  }
  if (now < pong.freezeUntil) return;
  if (now - pong.last < pong.ms) return;
  pong.last = now;

  int8_t nx = static_cast<int8_t>(pong.bx + pong.dx);
  int8_t ny = static_cast<int8_t>(pong.by + pong.dy);

  if (nx < 0 || nx > 7) {
    pong.dx = static_cast<int8_t>(-pong.dx);
    nx = static_cast<int8_t>(pong.bx + pong.dx);
    beepBtn();
  }

  if (ny >= 7 && pong.dy > 0) {
    if (nx >= pong.p1x && nx < static_cast<int8_t>(pong.p1x + pong.pw)) {
      pong.dy = -1;
      ny = 6;
      score += 1;
      beepBtn();
    } else {
      beepHit();
      finish(false, "Missed ball");
      return;
    }
  }

  if (ny <= 0 && pong.dy < 0) {
    if (nx >= pong.p2x && nx < static_cast<int8_t>(pong.p2x + pong.pw)) {
      pong.dy = 1;
      ny = 1;
      beepBtn();
    } else {
      score += 10;
      beepScore();
      pong.bx = 4;
      pong.by = 4;
      pong.dy = 1;
      pong.dx = (random(0, 2) == 0) ? -1 : 1;
      pong.freezeUntil = now + 350;
      if (pong.ms > 130) pong.ms = static_cast<uint16_t>(pong.ms - 6);
      return;
    }
  }

  pong.bx = nx;
  pong.by = ny;

  int8_t target = pong.bx;
  if (pong.p2x < target && pong.p2x < static_cast<int8_t>(8 - pong.pw)) {
    pong.p2x++;
  } else if (pong.p2x > target && pong.p2x > 0) {
    pong.p2x--;
  }
}
void pongDraw() {
  clearFrame();
  for (uint8_t i = 0; i < pong.pw; ++i) {
    px(static_cast<int8_t>(pong.p2x + i), 0, C(55, 0, 0));
    px(static_cast<int8_t>(pong.p1x + i), 7, C(0, 55, 0));
  }
  px(pong.bx, pong.by, C(65, 65, 65));
}

struct Tug {
  int8_t pos;
  uint32_t p1At;
  uint32_t p2At;
} tug;
void tugInit() {
  tug.pos = 4;
  tug.p1At = millis();
  tug.p2At = millis();
  score = 0;
}
void tugUpdate(uint32_t now) {
  if ((take(B_UP) || take(B_LEFT)) && now - tug.p1At >= 80) {
    tug.p1At = now;
    tug.pos--;
    score++;
    beepBtn();
  }
  if ((take(B_DOWN) || take(B_RIGHT) || take(B_PAUSE)) && now - tug.p2At >= 80) {
    tug.p2At = now;
    tug.pos++;
    score++;
    beepBtn();
  }
  if (tug.pos <= 0) {
    finish(true, "P1 wins");
    return;
  }
  if (tug.pos >= 7) {
    finish(true, "P2 wins");
  }
}
void tugDraw() {
  clearFrame();
  for (uint8_t x = 0; x < 8; ++x) px(x, 3, C(8, 8, 8));
  px(0, 3, C(0, 50, 0));
  px(7, 3, C(50, 0, 0));
  px(tug.pos, 3, C(65, 55, 0));
}

void initGame(GameId id) {
  switch (id) {
    case GAME_SNAKE_WALL:
      snakeInit(false);
      break;
    case GAME_SNAKE_WRAP:
      snakeInit(true);
      break;
    case GAME_TETRIS:
      tInit();
      break;
    case GAME_FLAAY_EASY:
      feInit();
      break;
    case GAME_FLAPPY_HARD:
      fInit();
      break;
    case GAME_ASTEROIDS_HARD:
      aInit();
      break;
    case GAME_PACMAN_EASY:
      peInit();
      break;
    case GAME_PACMAN_HARD:
      pInit();
      break;
    case GAME_SPACE_SHOOTER:
      shInit();
      break;
    case GAME_BREAKOUT:
      brInit();
      break;
    case GAME_TTT_AI:
      tttInit(true);
      break;
    case GAME_TTT_2P:
      tttInit(false);
      break;
    case GAME_PONG:
      pongInit();
      break;
    case GAME_TUG:
      tugInit();
      break;
    default:
      break;
  }
}
void updateGame(GameId id, uint32_t now) {
  switch (id) {
    case GAME_SNAKE_WALL:
    case GAME_SNAKE_WRAP:
      snakeUpdate(now);
      break;
    case GAME_TETRIS:
      tUpdate(now);
      break;
    case GAME_FLAAY_EASY:
      feUpdate(now);
      break;
    case GAME_FLAPPY_HARD:
      fUpdate(now);
      break;
    case GAME_ASTEROIDS_HARD:
      aUpdate(now);
      break;
    case GAME_PACMAN_EASY:
      peUpdate(now);
      break;
    case GAME_PACMAN_HARD:
      pUpdate(now);
      break;
    case GAME_SPACE_SHOOTER:
      shUpdate(now);
      break;
    case GAME_BREAKOUT:
      brUpdate(now);
      break;
    case GAME_TTT_AI:
    case GAME_TTT_2P:
      tttUpdate(now);
      break;
    case GAME_PONG:
      pongUpdate(now);
      break;
    case GAME_TUG:
      tugUpdate(now);
      break;
    default:
      break;
  }
}
void drawGame(GameId id) {
  switch (id) {
    case GAME_SNAKE_WALL:
    case GAME_SNAKE_WRAP:
      snakeDraw();
      break;
    case GAME_TETRIS:
      tDraw();
      break;
    case GAME_FLAAY_EASY:
      feDraw();
      break;
    case GAME_FLAPPY_HARD:
      fDraw();
      break;
    case GAME_ASTEROIDS_HARD:
      aDraw();
      break;
    case GAME_PACMAN_EASY:
      peDraw();
      break;
    case GAME_PACMAN_HARD:
      pDraw();
      break;
    case GAME_SPACE_SHOOTER:
      shDraw();
      break;
    case GAME_BREAKOUT:
      brDraw();
      break;
    case GAME_TTT_AI:
    case GAME_TTT_2P:
      tttDraw();
      break;
    case GAME_PONG:
      pongDraw();
      break;
    case GAME_TUG:
      tugDraw();
      break;
    default:
      clearFrame();
      break;
  }
}

void drawMenuMatrix(uint32_t now) {
  clearFrame();
  uint8_t sweep = static_cast<uint8_t>((now / 140) % MATRIX_W);
  for (uint8_t y = 0; y < MATRIX_H; ++y) {
    for (uint8_t x = 0; x < MATRIX_W; ++x) {
      uint8_t r = static_cast<uint8_t>((x * 20 + now / 9) & 0x3F);
      uint8_t g = static_cast<uint8_t>((y * 18 + now / 13) & 0x3F);
      uint8_t b = static_cast<uint8_t>((x * 9 + y * 11 + now / 17) & 0x3F);
      if (x == sweep) r = 70;
      px(x, y, C(r / 2, g / 2, b / 2));
    }
  }
}
void drawOverMatrix(uint32_t now) {
  clearFrame();
  bool p = ((now / 240) % 2) == 0;
  for (uint8_t y = 0; y < MATRIX_H; ++y) {
    for (uint8_t x = 0; x < MATRIX_W; ++x) {
      if (((x + y) % 2 == 0) != p) continue;
      px(x, y, lastWin ? C(0, 45, 0) : C(55, 0, 0));
    }
  }
}
void drawMenuOLED() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  oled.setCursor(0, 0);
  oled.print("GameBoy RP2040");
  int8_t s = menuIndex - 2;
  if (s < 0) s = 0;
  int8_t ms = static_cast<int8_t>(GAME_COUNT - 5);
  if (ms < 0) ms = 0;
  if (s > ms) s = ms;
  for (uint8_t i = 0; i < 5; ++i) {
    int8_t g = static_cast<int8_t>(s + i);
    if (g >= GAME_COUNT) break;
    oled.setCursor(0, 14 + i * 10);
    oled.print(g == menuIndex ? "> " : "  ");
    oled.print(GAME_NAMES[g]);
































  }
  oled.display();
}
void drawPlayOLED() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.print(GAME_NAMES[currentGame]);
  oled.setCursor(0, 16);
  oled.print("Score: ");
  oled.print(score);
  oled.setCursor(0, 28);
  oled.print("Best: ");
  oled.print(highScore[currentGame]);
  oled.setCursor(0, 46);
  oled.print("SEL -> Menu");
  oled.display();
}
void drawOverOLED() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.print(lastWin ? "Round Complete" : "Game Over");
  oled.setCursor(0, 12);
  oled.print(GAME_NAMES[currentGame]);
  oled.setCursor(0, 24);
  oled.print(lastMsg);
  oled.setCursor(0, 36);
  oled.print("Score: ");
  oled.print(score);
  oled.setCursor(0, 48);
  oled.print("Best: ");
  oled.print(highScore[currentGame]);
  oled.display();
}

void setup() {
  pinMode(BUZZER, OUTPUT);
  noTone(BUZZER);
  Wire.setSDA(D4);
  Wire.setSCL(D5);
  Wire.begin();
  matrix.begin();
  matrix.clear();
  matrix.show();
  playBootMatrixSequence();
  oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  initButtons();
  randomSeed(static_cast<uint32_t>(analogRead(A0)) ^ micros());
  lastMatrixMs = millis();
  lastOledMs = millis();
}

void loop() {
  uint32_t now = millis();
  updateButtons(now);

  if (take(B_SELECT) && systemState != STATE_MENU) {
    beepBtn();
    backToMenu();
  }

 if (systemState == STATE_MENU) {
    if (take(B_UP)) {
      menuIndex = static_cast<int8_t>((menuIndex + GAME_COUNT - 1) % GAME_COUNT);
      beepBtn();
    }
    if (take(B_DOWN)) {
      menuIndex = static_cast<int8_t>((menuIndex + 1) % GAME_COUNT);
      beepBtn();
    }
    if (take(B_PAUSE)) {
      beepBtn();
      startGame(static_cast<GameId>(menuIndex));




































    
   }
  } else if (systemState == STATE_PLAYING) {
    updateGame(currentGame, now);
  } else {
    if (take(B_PAUSE)) {
      beepBtn();
    }
  }

  updateSound(now);

  if (now - lastMatrixMs >= MATRIX_FRAME_MS) {
    lastMatrixMs = now;
    if (systemState == STATE_MENU)
      drawMenuMatrix(now);
    else if (systemState == STATE_PLAYING)
      drawGame(currentGame);
    else
      drawOverMatrix(now);
    showFrame();
  }
  if (now - lastOledMs >= OLED_FRAME_MS) {
    lastOledMs = now;
    if (systemState == STATE_MENU)
      drawMenuOLED();
    else if (systemState == STATE_PLAYING)
      drawPlayOLED();
    else
      drawOverOLED();
  }
}
