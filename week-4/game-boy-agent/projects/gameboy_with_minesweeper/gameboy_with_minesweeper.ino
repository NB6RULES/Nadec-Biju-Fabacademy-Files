// ========================================
// Multi-Game Console for RP2040
// Fixed and Optimized Version
// ========================================

#include <Adafruit_NeoPixel.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

// ========================================
// Pin Definitions
// ========================================
#define MATRIX_PIN D10
#define BTN_UP D3
#define BTN_DOWN D9
#define BTN_LEFT D2
#define BTN_RIGHT D7
#define BTN_PAUSE D6
#define BUZZER D0
#define GAME_SELECTOR_BTN D1

// ========================================
// Hardware Constants
// ========================================
constexpr uint8_t MATRIX_W = 8;
constexpr uint8_t MATRIX_H = 8;
constexpr uint8_t MATRIX_COUNT = 64;
constexpr uint16_t SCREEN_W = 128;
constexpr uint16_t SCREEN_H = 64;
constexpr uint8_t OLED_ADDR = 0x3C;

// ========================================
// Timing Constants
// ========================================
namespace Timing {
  constexpr uint16_t BTN_DEBOUNCE_MS = 30;
  constexpr uint16_t MATRIX_FRAME_MS = 33;  // ~30 FPS
  constexpr uint16_t OLED_FRAME_MS = 100;   // ~10 FPS
  constexpr uint16_t GAME_OVER_DELAY_MS = 2000;  // Auto-return to menu after 2s
  
  // Game-specific timing
  constexpr uint16_t SNAKE_INIT_MS = 280;
  constexpr uint16_t TETRIS_INIT_MS = 640;
  constexpr uint16_t FLAPPY_HARD_MS = 125;
  constexpr uint16_t FLAPPY_EASY_MS = 200;
}

// ========================================
// Hardware Objects
// ========================================
Adafruit_NeoPixel matrix(MATRIX_COUNT, MATRIX_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, -1);

// ========================================
// System State
// ========================================
enum SystemState : uint8_t {
  STATE_MENU,
  STATE_PLAYING,
  STATE_GAME_OVER
};

enum GameId : uint8_t {
  GAME_SNAKE_WALL,
  GAME_SNAKE_WRAP,
  GAME_TETRIS,
  GAME_FLAPPY_EASY,      // Fixed typo
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
  GAME_CHECKERS_AI,      // NEW!
  GAME_CHECKERS_2P,      // Renamed from GAME_CHECKERS
  GAME_MINESWEEPER,      // Minesweeper!
  GAME_DINO,             // NEW! Dino Run
  GAME_COUNT
};

const char *GAME_NAMES[GAME_COUNT] = {
  "Snake (Wall)",
  "Snake (Wrap)",
  "Tetris",
  "Flappy Bird Easy",    // Fixed typo
  "Flappy Bird Hard",
  "Asteroids Hard",
  "Pac-Man Easy",
  "Pac-Man Hard",
  "Space Shooter",
  "Breakout",
  "TicTacToe AI",
  "TicTacToe 2P",
  "Pong",
  "Tug of War 2P",
  "Checkers AI",         // NEW!
  "Checkers 2P",
  "Minesweeper",
  "Dino Run"
};

// ========================================
// Button System
// ========================================
enum ButtonId : uint8_t {
  B_UP,
  B_DOWN,
  B_LEFT,
  B_RIGHT,
  B_PAUSE,
  B_SELECT,
  BUTTON_COUNT
};

const uint8_t BUTTON_PINS[BUTTON_COUNT] = {
  BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_PAUSE, GAME_SELECTOR_BTN
};

struct Button {
  uint8_t pin;
  bool stable;
  bool last;
  bool press;
  bool release;
  uint32_t changedAt;
};

Button buttons[BUTTON_COUNT];

// ========================================
// Sound System
// ========================================
struct ToneEvent {
  uint16_t freq;
  uint16_t duration;
};

constexpr uint8_t TONE_QUEUE_SIZE = 24;
ToneEvent toneQueue[TONE_QUEUE_SIZE];
uint8_t toneHead = 0;
uint8_t toneTail = 0;
bool toneActive = false;
uint32_t toneEndTime = 0;
bool soundMuted = false;

// ========================================
// Global Game State
// ========================================
SystemState systemState = STATE_MENU;
GameId currentGame = GAME_SNAKE_WALL;
int8_t menuIndex = 0;
int16_t score = 0;
int16_t highScore[GAME_COUNT] = {0};
bool lastWin = false;
char lastMsg[24] = "Game over";
bool gamePaused = false;
bool menuMuteComboLatched = false;
uint32_t gameOverStartTime = 0;  // NEW: Track when game over started

// Frame buffer
uint32_t frameBuffer[MATRIX_COUNT];
uint32_t lastMatrixMs = 0;
uint32_t lastOledMs = 0;

// ========================================
// Game State Structures
// ========================================

struct Bullet {
  int8_t x;
  int8_t y;
  bool on;
};

struct Snake {
  int8_t x[64];
  int8_t y[64];
  uint8_t length;
  int8_t dx;
  int8_t dy;
  int8_t nextDx;
  int8_t nextDy;
  int8_t foodX;
  int8_t foodY;
  bool wrapMode;
  uint16_t moveInterval;
  uint32_t lastMove;
};

struct Tetris {
  uint8_t board[8][8];
  int8_t pieceX;
  int8_t pieceY;
  uint8_t pieceType;
  uint8_t rotation;
  uint16_t dropInterval;
  uint32_t lastDrop;
  uint32_t softDropTime;
};

struct Flappy {
  int16_t y8;  // Y position * 8 for sub-pixel movement
  int16_t velocity8;
  int8_t pipeX;
  int8_t gapY;
  bool scoredPipe;
  uint16_t moveInterval;
  uint32_t lastMove;
};

struct FlappyEasy {
  int8_t birdY;
  int8_t velocity;
  int8_t pipeX;
  int8_t gapY;
  uint8_t gapSize;
  uint16_t moveInterval;
  uint32_t lastMove;
};

struct AstRock {
  int8_t x;
  int8_t y;
  bool active;
  uint16_t moveInterval;
  uint32_t lastMove;
};

struct AsteroidsHard {
  int8_t shipX;
  Bullet bullets[5];
  AstRock rocks[8];
  uint16_t spawnInterval;
  uint32_t lastMove;
  uint32_t lastBulletUpdate;
  uint32_t lastSpawn;
  uint32_t lastShot;
};

struct AstEasyBullet {
  int8_t x;
  int8_t y;
  int8_t dx;
  int8_t dy;
  bool active;
};

struct AstEasyRock {
  int8_t x;
  int8_t y;
  bool active;
};

struct AsteroidsEasy {
  int8_t shipX;
  int8_t shipY;
  AstEasyBullet bullets[3];
  AstEasyRock rocks[5];
  uint32_t lastMove;
  uint32_t lastSpawn;
  uint32_t lastBulletUpdate;
  uint32_t lastInput;
  uint32_t lastShot;
};

struct PacEasy {
  int8_t pacX;
  int8_t pacY;
  int8_t direction;
  int8_t ghostX;
  int8_t ghostY;
  uint8_t pellets[8][8];
  int16_t pelletsLeft;
  uint32_t lastPacMove;
  uint32_t lastGhostMove;
};

struct Pacman {
  uint8_t cells[8][8];
  int8_t pacX;
  int8_t pacY;
  int8_t ghostX;
  int8_t ghostY;
  int8_t dx;
  int8_t dy;
  int8_t nextDx;
  int8_t nextDy;
  uint16_t pelletsLeft;
  uint16_t pacInterval;
  uint16_t ghostInterval;
  uint32_t lastPacMove;
  uint32_t lastGhostMove;
  uint8_t worldIndex;
};

struct Enemy {
  int8_t x;
  int8_t y;
  bool active;
};

struct SpaceShooter {
  int8_t shipX;
  Bullet playerBullets[6];
  Bullet enemyBullets[5];
  Enemy enemies[8];
  uint16_t enemyInterval;
  uint16_t spawnInterval;
  uint32_t lastMove;
  uint32_t lastPlayerBullet;
  uint32_t lastEnemyBullet;
  uint32_t lastEnemyStep;
  uint32_t lastSpawn;
  uint32_t lastShot;
  uint32_t lastEnemyShoot;
};

struct Breakout {
  bool bricks[3][8];
  int8_t paddleX;
  int8_t ballX;
  int8_t ballY;
  int8_t velocityX;
  int8_t velocityY;
  uint16_t moveInterval;
  uint32_t lastMove;
  uint32_t lastPaddleMove;
};

struct TicTacToe {
  uint8_t board[3][3];
  uint8_t cursorX;
  uint8_t cursorY;
  uint8_t turn;
  bool aiMode;
  bool waitingForAI;
  uint32_t aiMoveTime;
};

struct Pong {
  int8_t playerX;        // Player paddle X position (bottom side, y=7)
  int8_t aiX;            // AI paddle X position (top side, y=0)
  int8_t ballX;
  int8_t ballY;
  int8_t velocityX;
  int8_t velocityY;
  uint8_t paddleSize;    // Paddle width
  uint16_t moveInterval;
  uint32_t lastMove;
  uint32_t lastPaddleMove;
  uint32_t lastAIMove;
};

struct TugOfWar {
  int8_t position;
  uint32_t player1LastPress;
  uint32_t player2LastPress;
};

struct Checkers {
  uint8_t board[8][8];    // 0=empty, 1=P1, 2=P1 king, 3=P2, 4=P2 king
  int8_t cursorX;
  int8_t cursorY;
  int8_t selectedX;       // -1 if no piece selected
  int8_t selectedY;
  uint8_t currentPlayer;  // 1 or 2
  bool mustCapture;       // If true, must make a capture move
  uint8_t validMoves[12][2];  // Valid destination squares for selected piece
  uint8_t validMoveCount;
  bool aiMode;            // NEW: true if playing against AI
  bool waitingForAI;      // NEW: true when AI is thinking
  uint32_t aiMoveTime;    // NEW: when AI should make its move
  int8_t aiLastFromX;     // Last AI move origin
  int8_t aiLastFromY;
  int8_t aiLastToX;       // Last AI move destination
  int8_t aiLastToY;
  bool aiBlinkVisible;
  uint32_t aiBlinkNextToggleAt;
  uint32_t aiBlinkEndsAt;
};

struct Minesweeper {
  uint8_t board[8][8];      // 0-8 = number of adjacent mines, 9 = mine
  uint8_t revealed[8][8];   // 0 = hidden, 1 = revealed, 2 = flagged
  int8_t cursorX;
  int8_t cursorY;
  uint8_t minesLeft;        // Flags remaining
  uint8_t cellsToReveal;    // Non-mine cells left to reveal
  bool firstClick;          // First click can't be a mine
};

struct Dino {
  int16_t playerY8;      // Player Y position * 8 for sub-pixel movement
  int16_t velocityY8;    // Player Y velocity * 8
  bool isCrouching;      // NEW: For dodging
  int8_t obstacleX;      // X position of the current obstacle
  int8_t obstacleY;      // Y position of obstacle (for flying ones)
  uint8_t obstacleH;     // Height of the obstacle
  uint8_t obstacleW;     // Width of the obstacle
  uint8_t obstacleType;  // NEW: 0 for ground, 1 for flying
  bool passedObstacle;   // If score was given for this obstacle
  uint16_t moveInterval; // How fast the game scrolls
  uint32_t lastMove;     // Time of last game tick
};

// ========================================
// Memory-Optimized Game State Union
// ========================================
union GameState {
  Snake snake;
  Tetris tetris;
  Flappy flappyHard;
  FlappyEasy flappyEasy;
  AsteroidsHard asteroidsHard;
  AsteroidsEasy asteroidsEasy;
  PacEasy pacEasy;
  Pacman pacman;
  SpaceShooter shooter;
  Breakout breakout;
  TicTacToe ttt;
  Pong pong;
  TugOfWar tug;
  Checkers checkers;     // NEW!
  Minesweeper minesweeper;
  Dino dino;
};

GameState gameState;

// ========================================
// Forward Declarations
// ========================================
void initGame(GameId id);
void updateGame(GameId id, uint32_t now);
void drawGame(GameId id);

// Dino Game forward declarations
void dinoInit();
void dinoUpdate(uint32_t now);
void dinoDraw();

// ========================================
// Utility Functions
// ========================================

// Creates an RGB color value for NeoPixel LEDs
// Parameters: r, g, b (0-255 each)
// Returns: 32-bit packed color value
static inline uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
  return matrix.Color(r, g, b);
}

// Converts (x,y) grid coordinates to physical LED index
// The LED strip is arranged in a serpentine (snake) pattern:
// Row 0: left-to-right, Row 1: right-to-left, Row 2: left-to-right, etc.
// This function handles the conversion automatically
uint8_t ledIndex(uint8_t x, uint8_t y) {
  return (y % 2 == 0) ?  // Even rows (0,2,4,6) go left-to-right
    static_cast<uint8_t>(y * MATRIX_W + x) : 
    static_cast<uint8_t>(y * MATRIX_W + (MATRIX_W - 1 - x));  // Odd rows reversed
}

// Clears the entire frame buffer to a single color (default: black/off)
// Call this at the start of each game's draw function
void clearFrame(uint32_t color = 0) {
  for (uint8_t i = 0; i < MATRIX_COUNT; ++i) {
    frameBuffer[i] = color;
  }
}

// Sets a single pixel in the frame buffer
// Bounds checking prevents drawing outside the 8x8 grid
// Changes are not visible until showFrame() is called
void setPixel(int8_t x, int8_t y, uint32_t color) {
  if (x < 0 || x >= MATRIX_W || y < 0 || y >= MATRIX_H) return;  // Ignore out-of-bounds
  frameBuffer[ledIndex(static_cast<uint8_t>(x), static_cast<uint8_t>(y))] = color;
}

// Pushes the frame buffer to the physical LEDs
// Call once per frame after all setPixel() calls
// This is when the display actually updates
void showFrame() {
  for (uint8_t i = 0; i < MATRIX_COUNT; ++i) {
    matrix.setPixelColor(i, frameBuffer[i]);
  }
  matrix.show();  // Send data to NeoPixels (takes ~2ms)
}

void drawFlashingPauseBorder(uint32_t now) {
  if (!gamePaused) return;
  if (((now / 200) % 2) != 0) return;
  
  uint32_t borderColor = Color(50, 50, 0);
  for (uint8_t i = 0; i < 8; ++i) {
    setPixel(0, i, borderColor);
    setPixel(7, i, borderColor);
    setPixel(i, 0, borderColor);
    setPixel(i, 7, borderColor);
  }
}

void setSoundMuted(bool muted) {
  soundMuted = muted;
  if (soundMuted) {
    noTone(BUZZER);
    toneActive = false;
    toneHead = 0;
    toneTail = 0;
  }
}

// ========================================
// Sound Functions
// ========================================

// Adds a tone to the non-blocking sound queue
// Traditional tone() blocks code - this queues sounds for later playback
// Queue is circular buffer with head/tail pointers
void enqueueTone(uint16_t freq, uint16_t duration) {
  if (soundMuted) return;
  
  uint8_t nextTail = static_cast<uint8_t>((toneTail + 1) % TONE_QUEUE_SIZE);
  if (nextTail == toneHead) return;  // Queue full - discard sound
  
  toneQueue[toneTail] = {freq, duration};
  toneTail = nextTail;
}

// Predefined sound effects for common game events
// These can be chained by calling multiple times - they'll play in sequence
void beepButton() { enqueueTone(1200, 26); }  // UI feedback - short high beep
void beepScore() { enqueueTone(1600, 35); }   // Scoring - higher pitched
void beepHit() { enqueueTone(240, 90); }      // Damage - low rumble

// Multi-tone sound sequences for game state changes
void beepStart() {
  enqueueTone(500, 45);   // Ascending
  enqueueTone(850, 45);   // three-tone
  enqueueTone(1200, 65);  // fanfare
}

void beepLose() {
  enqueueTone(1000, 60);  // Descending
  enqueueTone(700, 70);   // three-tone
  enqueueTone(430, 90);   // sad sound
}

void beepWin() {
  enqueueTone(800, 50);   // Ascending
  enqueueTone(1050, 50);  // three-tone
  enqueueTone(1400, 80);  // victory sound
}

void updateSound(uint32_t now) {
  if (soundMuted) {
    if (toneActive) {
      noTone(BUZZER);
      toneActive = false;
    }
    toneHead = toneTail;
    return;
  }
  
  if (toneActive && now >= toneEndTime) {
    noTone(BUZZER);
    toneActive = false;
  }
  
  if (!toneActive && toneHead != toneTail) {
    ToneEvent event = toneQueue[toneHead];
    toneHead = (toneHead + 1) % TONE_QUEUE_SIZE;
    tone(BUZZER, event.freq);
    toneEndTime = now + event.duration;
    toneActive = true;
  }
}

// ========================================
// Button Functions
// ========================================

void initButtons() {
  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    bool pressed = digitalRead(BUTTON_PINS[i]) == LOW;
    buttons[i] = {BUTTON_PINS[i], pressed, pressed, false, false, millis()};
  }
}

void clearButtonEdges() {
  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
    buttons[i].press = buttons[i].release = false;
  }
}

void updateButtons(uint32_t now) {
  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
    bool raw = digitalRead(buttons[i].pin) == LOW;
    
    if (raw != buttons[i].last) {
      buttons[i].last = raw;
      buttons[i].changedAt = now;
    }
    
    if (now - buttons[i].changedAt >= Timing::BTN_DEBOUNCE_MS && raw != buttons[i].stable) {
      buttons[i].stable = raw;
      if (raw) {
        buttons[i].press = true;
      } else {
        buttons[i].release = true;
      }
    }
  }
}

bool takePress(ButtonId id) {
  if (!buttons[id].press) return false;
  buttons[id].press = false;
  return true;
}

bool isDown(ButtonId id) {
  return buttons[id].stable;
}

// ========================================
// State Management
// ========================================

void returnToMenu() {
  systemState = STATE_MENU;
  gamePaused = false;
  menuMuteComboLatched = false;
  clearButtonEdges();
}

void finishGame(bool win, const char *message) {
  if (systemState != STATE_PLAYING) return;
  
  if (score > highScore[currentGame]) {
    highScore[currentGame] = score;
  }
  
  lastWin = win;
  
  if (message) {
    strncpy(lastMsg, message, sizeof(lastMsg) - 1);
  } else {
    strncpy(lastMsg, win ? "Win" : "Game over", sizeof(lastMsg) - 1);
  }
  lastMsg[sizeof(lastMsg) - 1] = '\0';  // Ensure null termination
  
  systemState = STATE_GAME_OVER;
  gameOverStartTime = millis();  // NEW: Record when game over started
  clearButtonEdges();
  
  if (win) {
    beepWin();
  } else {
    beepLose();
  }
}

void startGame(GameId id) {
  currentGame = id;
  score = 0;
  lastWin = false;
  gamePaused = false;
  menuMuteComboLatched = false;
  strncpy(lastMsg, "Game over", sizeof(lastMsg) - 1);
  lastMsg[sizeof(lastMsg) - 1] = '\0';
  
  systemState = STATE_PLAYING;
  clearButtonEdges();
  initGame(id);
  beepStart();
}

// ========================================
// Boot Animation
// ========================================

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

// Pac-Man sprites (mouth open and closed)
// Mouth facing LEFT (for entering from left side)
const uint8_t GLYPH_PACMAN_OPEN[8] = {
  0b00111100,  // Center (symmetric)
  0b00011110,  // Mouth opens to the left
  0b00001111,
  0b00000111,
  0b00000111,
  0b00001111,
  0b00011110,
  0b00111100,
};

const uint8_t GLYPH_PACMAN_CLOSED[8] = {
  0b00111100,
  0b01111110,
  0b11111111,
  0b11111111,
  0b11111111,
  0b11111111,
  0b01111110,
  0b00111100,
};

void drawGlyph(const uint8_t rows[8], uint32_t color) {
  clearFrame();
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      if (rows[y] & static_cast<uint8_t>(1 << x)) {
        setPixel(static_cast<int8_t>(x), static_cast<int8_t>(y), color);
      }
    }
  }
  showFrame();
}

void drawPacmanAt(int8_t xPos, bool mouthOpen, uint32_t color) {
  clearFrame();
  const uint8_t* sprite = mouthOpen ? GLYPH_PACMAN_OPEN : GLYPH_PACMAN_CLOSED;
  
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      if (sprite[y] & static_cast<uint8_t>(1 << x)) {
        int8_t drawX = xPos + static_cast<int8_t>(x);
        if (drawX >= 0 && drawX < 8) {
          setPixel(drawX, static_cast<int8_t>(y), color);
        }
      }
    }
  }
  showFrame();
}

void drawPacmanAndHeart(int8_t pacX, bool mouthOpen, bool showHeart, uint32_t pacColor, uint32_t heartColor) {
  clearFrame();
  
  // Draw heart first (if visible)
  if (showHeart) {
    for (uint8_t y = 0; y < 8; ++y) {
      for (uint8_t x = 0; x < 8; ++x) {
        if (GLYPH_HEART[y] & static_cast<uint8_t>(1 << x)) {
          setPixel(static_cast<int8_t>(x), static_cast<int8_t>(y), heartColor);
        }
      }
    }
  }
  
  // Draw Pac-Man on top
  const uint8_t* sprite = mouthOpen ? GLYPH_PACMAN_OPEN : GLYPH_PACMAN_CLOSED;
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      if (sprite[y] & static_cast<uint8_t>(1 << x)) {
        int8_t drawX = pacX + static_cast<int8_t>(x);
        if (drawX >= 0 && drawX < 8) {
          setPixel(drawX, static_cast<int8_t>(y), pacColor);
        }
      }
    }
  }
  showFrame();
}

void playBootSequence() {
  constexpr uint16_t frameMs = 1333;
  
  drawGlyph(GLYPH_N, Color(0, 80, 255));
  delay(frameMs);
  
  drawGlyph(GLYPH_B, Color(255, 85, 0));
  delay(frameMs);
  
  drawGlyph(GLYPH_6, Color(80, 255, 80));
  delay(frameMs);
  
  // Heartbeat animation for 2 seconds
  const uint8_t levels[8] = {30, 60, 95, 140, 200, 140, 95, 60};
  uint32_t start = millis();
  uint8_t i = 0;
  while (millis() - start < 2000) {  // Changed from 4000 to 2000
    uint8_t intensity = levels[i % 8];
    drawGlyph(GLYPH_HEART, Color(intensity, 0, 0));
    delay(125);
    ++i;
  }
  
  // Pac-Man eating animation
  const uint32_t pacColor = Color(255, 200, 0);  // Yellow Pac-Man
  const uint32_t heartColor = Color(200, 0, 0);   // Red heart
  
  // Pac-Man approaches from the left
  for (int8_t x = -8; x <= 0; x++) {
    bool mouthOpen = ((x / 2) % 2) == 0;  // Animate mouth
    drawPacmanAndHeart(x, mouthOpen, true, pacColor, heartColor);
    delay(60);
  }
  
  // Pac-Man "eats" the heart (heart disappears)
  drawPacmanAndHeart(0, true, true, pacColor, heartColor);
  delay(150);
  drawPacmanAndHeart(0, false, false, pacColor, heartColor);  // Mouth closes, heart gone
  delay(100);
  
  // Pac-Man continues off screen to the right
  for (int8_t x = 1; x <= 8; x++) {
    bool mouthOpen = ((x / 2) % 2) == 0;
    drawPacmanAt(x, mouthOpen, pacColor);
    delay(60);
  }
  
  clearFrame();
  showFrame();
}

// ========================================
// SNAKE GAME
// ========================================

bool snakeHasSegment(int8_t x, int8_t y, uint8_t length) {
  for (uint8_t i = 0; i < length; ++i) {
    if (gameState.snake.x[i] == x && gameState.snake.y[i] == y) {
      return true;
    }
  }
  return false;
}

void snakePlaceFood() {
  for (uint8_t attempts = 0; attempts < 100; ++attempts) {
    int8_t x = static_cast<int8_t>(random(0, 8));
    int8_t y = static_cast<int8_t>(random(0, 8));
    if (!snakeHasSegment(x, y, gameState.snake.length)) {
      gameState.snake.foodX = x;
      gameState.snake.foodY = y;
      return;
    }
  }
  gameState.snake.foodX = 0;
  gameState.snake.foodY = 0;
}

void snakeInit(bool wrapMode) {
  gameState.snake.length = 3;
  gameState.snake.x[0] = 4;
  gameState.snake.y[0] = 4;
  gameState.snake.x[1] = 3;
  gameState.snake.y[1] = 4;
  gameState.snake.x[2] = 2;
  gameState.snake.y[2] = 4;
  gameState.snake.dx = 1;
  gameState.snake.dy = 0;
  gameState.snake.nextDx = 1;
  gameState.snake.nextDy = 0;
  gameState.snake.wrapMode = wrapMode;
  gameState.snake.moveInterval = Timing::SNAKE_INIT_MS;
  gameState.snake.lastMove = millis();
  snakePlaceFood();
}

void snakeUpdate(uint32_t now) {
  if (takePress(B_PAUSE)) {
    gamePaused = !gamePaused;
    beepButton();
  }
  
  if (gamePaused) return;
  
  if (takePress(B_UP) && gameState.snake.dy != 1) {
    gameState.snake.nextDx = 0;
    gameState.snake.nextDy = -1;
    beepButton();
  }
  if (takePress(B_DOWN) && gameState.snake.dy != -1) {
    gameState.snake.nextDx = 0;
    gameState.snake.nextDy = 1;
    beepButton();
  }
  if (takePress(B_LEFT) && gameState.snake.dx != 1) {
    gameState.snake.nextDx = -1;
    gameState.snake.nextDy = 0;
    beepButton();
  }
  if (takePress(B_RIGHT) && gameState.snake.dx != -1) {
    gameState.snake.nextDx = 1;
    gameState.snake.nextDy = 0;
    beepButton();
  }
  
  if (now - gameState.snake.lastMove < gameState.snake.moveInterval) return;
  gameState.snake.lastMove = now;
  
  gameState.snake.dx = gameState.snake.nextDx;
  gameState.snake.dy = gameState.snake.nextDy;
  
  int8_t newX = static_cast<int8_t>(gameState.snake.x[0] + gameState.snake.dx);
  int8_t newY = static_cast<int8_t>(gameState.snake.y[0] + gameState.snake.dy);
  
  if (gameState.snake.wrapMode) {
    if (newX < 0) newX = 7;
    if (newX > 7) newX = 0;
    if (newY < 0) newY = 7;
    if (newY > 7) newY = 0;
  } else if (newX < 0 || newX > 7 || newY < 0 || newY > 7) {
    beepHit();
    finishGame(false, "Hit wall");
    return;
  }
  
  if (snakeHasSegment(newX, newY, gameState.snake.length)) {
    beepHit();
    finishGame(false, "Hit body");
    return;
  }
  
  bool ateFood = (newX == gameState.snake.foodX && newY == gameState.snake.foodY);
  if (ateFood && gameState.snake.length < 64) {
    gameState.snake.length++;
  }
  
  for (int8_t i = static_cast<int8_t>(gameState.snake.length - 1); i > 0; --i) {
    gameState.snake.x[i] = gameState.snake.x[i - 1];
    gameState.snake.y[i] = gameState.snake.y[i - 1];
  }
  
  gameState.snake.x[0] = newX;
  gameState.snake.y[0] = newY;
  
  if (ateFood) {
    score += 10;
    beepScore();
    
    if (gameState.snake.length == 64) {
      finishGame(true, "Board full");
      return;
    }
    
    snakePlaceFood();
    
    if (gameState.snake.moveInterval > 100) {
      gameState.snake.moveInterval = static_cast<uint16_t>(gameState.snake.moveInterval - 10);
    }
  }
}

void snakeDraw() {
  clearFrame();
  
  uint32_t headColor = gameState.snake.wrapMode ? Color(150, 20, 150) : Color(30, 130, 30);
  uint32_t bodyColor = gameState.snake.wrapMode ? Color(90, 35, 90) : Color(20, 70, 20);
  uint32_t foodColor = gameState.snake.wrapMode ? Color(0, 85, 85) : Color(85, 12, 12);
  
  setPixel(gameState.snake.foodX, gameState.snake.foodY, foodColor);
  
  for (int8_t i = static_cast<int8_t>(gameState.snake.length - 1); i >= 0; --i) {
    setPixel(gameState.snake.x[i], gameState.snake.y[i], i == 0 ? headColor : bodyColor);
  }
}

// ========================================
// TETRIS GAME
// ========================================

struct TetrisRotation {
  int8_t x[4];
  int8_t y[4];
};

const TetrisRotation TETRIS_SHAPES[5][4] = {
  // I piece
  {{{0, 1, 2, 3}, {1, 1, 1, 1}},
   {{2, 2, 2, 2}, {0, 1, 2, 3}},
   {{0, 1, 2, 3}, {2, 2, 2, 2}},
   {{1, 1, 1, 1}, {0, 1, 2, 3}}},
  
  // O piece
  {{{1, 2, 1, 2}, {0, 0, 1, 1}},
   {{1, 2, 1, 2}, {0, 0, 1, 1}},
   {{1, 2, 1, 2}, {0, 0, 1, 1}},
   {{1, 2, 1, 2}, {0, 0, 1, 1}}},
  
  // T piece
  {{{1, 0, 1, 2}, {0, 1, 1, 1}},
   {{1, 1, 2, 1}, {0, 1, 1, 2}},
   {{0, 1, 2, 1}, {1, 1, 1, 2}},
   {{1, 0, 1, 1}, {0, 1, 1, 2}}},
  
  // L piece
  {{{0, 0, 0, 1}, {0, 1, 2, 2}},
   {{0, 1, 2, 2}, {1, 1, 1, 0}},
   {{0, 1, 1, 1}, {0, 0, 1, 2}},
   {{0, 0, 1, 2}, {2, 1, 1, 1}}},
  
  // Z piece
  {{{1, 2, 0, 1}, {0, 0, 1, 1}},
   {{1, 1, 2, 2}, {0, 1, 1, 2}},
   {{1, 2, 0, 1}, {1, 1, 2, 2}},
   {{0, 0, 1, 1}, {0, 1, 1, 2}}}
};

bool tetrisCanPlace(int8_t x, int8_t y, uint8_t type, uint8_t rotation) {
  for (uint8_t i = 0; i < 4; ++i) {
    int8_t blockX = static_cast<int8_t>(x + TETRIS_SHAPES[type][rotation].x[i]);
    int8_t blockY = static_cast<int8_t>(y + TETRIS_SHAPES[type][rotation].y[i]);
    
    if (blockX < 0 || blockX > 7 || blockY < 0 || blockY > 7) return false;
    if (gameState.tetris.board[blockY][blockX]) return false;
  }
  return true;
}

void tetrisSpawnPiece() {
  gameState.tetris.pieceType = static_cast<uint8_t>(random(0, 5));
  gameState.tetris.rotation = 0;
  gameState.tetris.pieceX = 2;
  gameState.tetris.pieceY = 0;
  
  if (!tetrisCanPlace(gameState.tetris.pieceX, gameState.tetris.pieceY, 
                      gameState.tetris.pieceType, gameState.tetris.rotation)) {
    beepHit();
    finishGame(false, "Stacked out");
  }
}

void tetrisLockPiece() {
  for (uint8_t i = 0; i < 4; ++i) {
    int8_t x = static_cast<int8_t>(gameState.tetris.pieceX + TETRIS_SHAPES[gameState.tetris.pieceType][gameState.tetris.rotation].x[i]);
    int8_t y = static_cast<int8_t>(gameState.tetris.pieceY + TETRIS_SHAPES[gameState.tetris.pieceType][gameState.tetris.rotation].y[i]);
    
    if (x >= 0 && x < 8 && y >= 0 && y < 8) {
      gameState.tetris.board[y][x] = static_cast<uint8_t>(gameState.tetris.pieceType + 1);
    }
  }
}

void tetrisClearLines() {
  uint8_t linesCleared = 0;
  
  for (int8_t y = 7; y >= 0; --y) {
    bool full = true;
    for (uint8_t x = 0; x < 8; ++x) {
      if (!gameState.tetris.board[y][x]) {
        full = false;
        break;
      }
    }
    
    if (!full) continue;
    
    linesCleared++;
    
    for (int8_t row = y; row > 0; --row) {
      for (uint8_t x = 0; x < 8; ++x) {
        gameState.tetris.board[row][x] = gameState.tetris.board[row - 1][x];
      }
    }
    
    for (uint8_t x = 0; x < 8; ++x) {
      gameState.tetris.board[0][x] = 0;
    }
    
    y++;  // Recheck this row
  }
  
  if (linesCleared) {
    score += static_cast<int16_t>(linesCleared * 10);
    beepScore();
  }
}

void tetrisDropPiece() {
  if (tetrisCanPlace(gameState.tetris.pieceX, 
                     static_cast<int8_t>(gameState.tetris.pieceY + 1),
                     gameState.tetris.pieceType, 
                     gameState.tetris.rotation)) {
    gameState.tetris.pieceY++;
    return;
  }
  
  tetrisLockPiece();
  tetrisClearLines();
  tetrisSpawnPiece();
}

void tetrisInit() {
  memset(gameState.tetris.board, 0, sizeof(gameState.tetris.board));
  gameState.tetris.dropInterval = Timing::TETRIS_INIT_MS;
  gameState.tetris.lastDrop = millis();
  gameState.tetris.softDropTime = millis();
  tetrisSpawnPiece();
}

void tetrisUpdate(uint32_t now) {
  if (takePress(B_LEFT) && tetrisCanPlace(static_cast<int8_t>(gameState.tetris.pieceX - 1),
                                          gameState.tetris.pieceY,
                                          gameState.tetris.pieceType,
                                          gameState.tetris.rotation)) {
    gameState.tetris.pieceX--;
    beepButton();
  }
  
  if (takePress(B_RIGHT) && tetrisCanPlace(static_cast<int8_t>(gameState.tetris.pieceX + 1),
                                           gameState.tetris.pieceY,
                                           gameState.tetris.pieceType,
                                           gameState.tetris.rotation)) {
    gameState.tetris.pieceX++;
    beepButton();
  }
  
  if (takePress(B_PAUSE)) {
    uint8_t newRotation = static_cast<uint8_t>((gameState.tetris.rotation + 1) % 4);
    if (tetrisCanPlace(gameState.tetris.pieceX, gameState.tetris.pieceY,
                      gameState.tetris.pieceType, newRotation)) {
      gameState.tetris.rotation = newRotation;
      beepButton();
    }
  }
  
  if (takePress(B_DOWN) || (isDown(B_DOWN) && now - gameState.tetris.softDropTime >= 90)) {
    gameState.tetris.softDropTime = now;
    tetrisDropPiece();
  }
  
  if (now - gameState.tetris.lastDrop >= gameState.tetris.dropInterval) {
    gameState.tetris.lastDrop = now;
    tetrisDropPiece();
  }
  
  int16_t newInterval = static_cast<int16_t>(640 - score * 3);
  if (newInterval < 120) newInterval = 120;
  gameState.tetris.dropInterval = static_cast<uint16_t>(newInterval);
}

void tetrisDraw() {
  clearFrame();
  
  const uint32_t colors[6] = {
    0,
    0x000022,
    0x221100,
    0x002200,
    0x220000,
    0x001122
  };
  
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      if (gameState.tetris.board[y][x]) {
        uint32_t c = colors[gameState.tetris.board[y][x]];
        setPixel(x, y, Color(
          static_cast<uint8_t>((c >> 16) & 0xFF),
          static_cast<uint8_t>((c >> 8) & 0xFF),
          static_cast<uint8_t>(c & 0xFF)
        ));
      }
    }
  }
  
  for (uint8_t i = 0; i < 4; ++i) {
    setPixel(
      static_cast<int8_t>(gameState.tetris.pieceX + TETRIS_SHAPES[gameState.tetris.pieceType][gameState.tetris.rotation].x[i]),
      static_cast<int8_t>(gameState.tetris.pieceY + TETRIS_SHAPES[gameState.tetris.pieceType][gameState.tetris.rotation].y[i]),
      Color(60, 60, 60)
    );
  }
}

// ========================================
// FLAPPY BIRD (HARD) GAME
// ========================================

void flappyHardNewPipe() {
  gameState.flappyHard.pipeX = 8;
  gameState.flappyHard.gapY = static_cast<int8_t>(random(1, 5));
  gameState.flappyHard.scoredPipe = false;
}

void flappyHardInit() {
  gameState.flappyHard.y8 = 28;
  gameState.flappyHard.velocity8 = 0;
  gameState.flappyHard.moveInterval = Timing::FLAPPY_HARD_MS;
  gameState.flappyHard.lastMove = millis();
  flappyHardNewPipe();
}

void flappyHardUpdate(uint32_t now) {
  if (takePress(B_UP) || takePress(B_PAUSE)) {
    gameState.flappyHard.velocity8 = -14;
    beepButton();
  }
  
  if (now - gameState.flappyHard.lastMove < gameState.flappyHard.moveInterval) return;
  gameState.flappyHard.lastMove = now;
  
  gameState.flappyHard.velocity8 += 4;
  if (gameState.flappyHard.velocity8 > 18) {
    gameState.flappyHard.velocity8 = 18;
  }
  
  gameState.flappyHard.y8 = static_cast<int16_t>(gameState.flappyHard.y8 + gameState.flappyHard.velocity8);
  int8_t birdY = static_cast<int8_t>(gameState.flappyHard.y8 / 8);
  
  gameState.flappyHard.pipeX--;
  if (gameState.flappyHard.pipeX < -2) {
    flappyHardNewPipe();
  }
  
  if (!gameState.flappyHard.scoredPipe && gameState.flappyHard.pipeX + 1 < 2) {
    gameState.flappyHard.scoredPipe = true;
    score++;
    beepScore();
  }
  
  if (gameState.flappyHard.y8 < 0 || birdY > 7) {
    beepHit();
    finishGame(false, "Crashed");
    return;
  }
  
  if (2 >= gameState.flappyHard.pipeX && 2 < gameState.flappyHard.pipeX + 2) {
    if (birdY < gameState.flappyHard.gapY || birdY >= gameState.flappyHard.gapY + 3) {
      beepHit();
      finishGame(false, "Hit pipe");
      return;
    }
  }
  
  int16_t newInterval = static_cast<int16_t>(125 - score * 2);
  if (newInterval < 65) newInterval = 65;
  gameState.flappyHard.moveInterval = static_cast<uint16_t>(newInterval);
}

void flappyHardDraw() {
  clearFrame();
  
  for (int8_t x = gameState.flappyHard.pipeX; x < gameState.flappyHard.pipeX + 2; ++x) {
    for (uint8_t y = 0; y < 8; ++y) {
      if (y < gameState.flappyHard.gapY || y >= gameState.flappyHard.gapY + 3) {
        setPixel(x, y, Color(0, 35, 8));
      }
    }
  }
  
  setPixel(2, static_cast<int8_t>(gameState.flappyHard.y8 / 8), Color(70, 55, 0));
}

// ========================================
// FLAPPY BIRD (EASY) GAME
// ========================================

void flappyEasyInit() {
  gameState.flappyEasy.birdY = 4;
  gameState.flappyEasy.velocity = 0;
  gameState.flappyEasy.pipeX = 7;
  gameState.flappyEasy.gapY = static_cast<int8_t>(random(1, 5));
  gameState.flappyEasy.gapSize = 3;
  gameState.flappyEasy.moveInterval = Timing::FLAPPY_EASY_MS;
  gameState.flappyEasy.lastMove = millis();
}

void flappyEasyUpdate(uint32_t now) {
  if (takePress(B_UP) || takePress(B_PAUSE)) {
    gameState.flappyEasy.velocity = -2;
    beepButton();
  }
  
  if (now - gameState.flappyEasy.lastMove < gameState.flappyEasy.moveInterval) return;
  gameState.flappyEasy.lastMove = now;
  
  gameState.flappyEasy.velocity++;
  gameState.flappyEasy.birdY = static_cast<int8_t>(gameState.flappyEasy.birdY + gameState.flappyEasy.velocity);
  
  if (gameState.flappyEasy.birdY < 0 || gameState.flappyEasy.birdY >= 8) {
    beepHit();
    finishGame(false, "Crashed");
    return;
  }
  
  gameState.flappyEasy.pipeX--;
  if (gameState.flappyEasy.pipeX < 0) {
    gameState.flappyEasy.pipeX = 7;
    gameState.flappyEasy.gapY = static_cast<int8_t>(random(1, 5));
    score += 10;
    beepScore();
  }
  
  if (gameState.flappyEasy.pipeX == 0) {
    if (gameState.flappyEasy.birdY < gameState.flappyEasy.gapY || 
        gameState.flappyEasy.birdY >= gameState.flappyEasy.gapY + gameState.flappyEasy.gapSize) {
      beepHit();
      finishGame(false, "Hit pipe");
      return;
    }
  }
}

void flappyEasyDraw() {
  clearFrame();
  
  setPixel(0, gameState.flappyEasy.birdY, Color(70, 55, 0));
  
  for (uint8_t y = 0; y < 8; ++y) {
    if (y < gameState.flappyEasy.gapY || y >= gameState.flappyEasy.gapY + gameState.flappyEasy.gapSize) {
      setPixel(gameState.flappyEasy.pipeX, static_cast<int8_t>(y), Color(0, 55, 0));
    }
  }
}

// ========================================
// ASTEROIDS (HARD) GAME
// ========================================

void asteroidsHardInit() {
  gameState.asteroidsHard.shipX = 3;
  gameState.asteroidsHard.spawnInterval = 900;
  gameState.asteroidsHard.lastMove = millis();
  gameState.asteroidsHard.lastBulletUpdate = millis();
  gameState.asteroidsHard.lastSpawn = millis();
  gameState.asteroidsHard.lastShot = millis();
  
  for (uint8_t i = 0; i < 5; ++i) {
    gameState.asteroidsHard.bullets[i].on = false;
  }
  for (uint8_t i = 0; i < 8; ++i) {
    gameState.asteroidsHard.rocks[i].active = false;
  }
}

void asteroidsHardShoot() {
  for (uint8_t i = 0; i < 5; ++i) {
    if (!gameState.asteroidsHard.bullets[i].on) {
      gameState.asteroidsHard.bullets[i] = {gameState.asteroidsHard.shipX, 6, true};
      return;
    }
  }
}

void asteroidsHardSpawnRock() {
  for (uint8_t i = 0; i < 8; ++i) {
    if (!gameState.asteroidsHard.rocks[i].active) {
      int16_t interval = static_cast<int16_t>(random(180, 320) - score * 2);
      if (interval < 90) interval = 90;
      
      gameState.asteroidsHard.rocks[i] = {
        static_cast<int8_t>(random(0, 8)),
        0,
        true,
        static_cast<uint16_t>(interval),
        millis()
      };
      return;
    }
  }
}

void asteroidsHardUpdate(uint32_t now) {
  if (isDown(B_LEFT) && now - gameState.asteroidsHard.lastMove >= 90) {
    gameState.asteroidsHard.lastMove = now;
    if (gameState.asteroidsHard.shipX > 0) {
      gameState.asteroidsHard.shipX--;
      beepButton();
    }
  }
  
  if (isDown(B_RIGHT) && now - gameState.asteroidsHard.lastMove >= 90) {
    gameState.asteroidsHard.lastMove = now;
    if (gameState.asteroidsHard.shipX < 7) {
      gameState.asteroidsHard.shipX++;
      beepButton();
    }
  }
  
  if ((takePress(B_UP) || takePress(B_PAUSE)) && now - gameState.asteroidsHard.lastShot >= 170) {
    gameState.asteroidsHard.lastShot = now;
    asteroidsHardShoot();
    beepButton();
  }
  
  if (now - gameState.asteroidsHard.lastBulletUpdate >= 85) {
    gameState.asteroidsHard.lastBulletUpdate = now;
    for (uint8_t i = 0; i < 5; ++i) {
      if (gameState.asteroidsHard.bullets[i].on) {
        gameState.asteroidsHard.bullets[i].y--;
        if (gameState.asteroidsHard.bullets[i].y < 0) {
          gameState.asteroidsHard.bullets[i].on = false;
        }
      }
    }
  }
  
  if (now - gameState.asteroidsHard.lastSpawn >= gameState.asteroidsHard.spawnInterval) {
    gameState.asteroidsHard.lastSpawn = now;
    asteroidsHardSpawnRock();
  }
  
  for (uint8_t i = 0; i < 8; ++i) {
    if (gameState.asteroidsHard.rocks[i].active && 
        now - gameState.asteroidsHard.rocks[i].lastMove >= gameState.asteroidsHard.rocks[i].moveInterval) {
      gameState.asteroidsHard.rocks[i].lastMove = now;
      gameState.asteroidsHard.rocks[i].y++;
      if (gameState.asteroidsHard.rocks[i].y > 7) {
        gameState.asteroidsHard.rocks[i].active = false;
      }
    }
  }
  
  for (uint8_t bi = 0; bi < 5; ++bi) {
    if (gameState.asteroidsHard.bullets[bi].on) {
      for (uint8_t ri = 0; ri < 8; ++ri) {
        if (gameState.asteroidsHard.rocks[ri].active &&
            gameState.asteroidsHard.bullets[bi].x == gameState.asteroidsHard.rocks[ri].x &&
            gameState.asteroidsHard.bullets[bi].y == gameState.asteroidsHard.rocks[ri].y) {
          gameState.asteroidsHard.bullets[bi].on = false;
          gameState.asteroidsHard.rocks[ri].active = false;
          score++;
          beepScore();
          break;
        }
      }
    }
  }
  
  for (uint8_t i = 0; i < 8; ++i) {
    if (gameState.asteroidsHard.rocks[i].active &&
        gameState.asteroidsHard.rocks[i].x == gameState.asteroidsHard.shipX &&
        gameState.asteroidsHard.rocks[i].y == 7) {
      beepHit();
      finishGame(false, "Ship hit");
      return;
    }
  }
  
  int16_t newInterval = static_cast<int16_t>(900 - score * 16);
  if (newInterval < 220) newInterval = 220;
  gameState.asteroidsHard.spawnInterval = static_cast<uint16_t>(newInterval);
}

void asteroidsHardDraw() {
  clearFrame();
  
  for (uint8_t i = 0; i < 8; ++i) {
    if (gameState.asteroidsHard.rocks[i].active) {
      setPixel(gameState.asteroidsHard.rocks[i].x, gameState.asteroidsHard.rocks[i].y, Color(40, 18, 0));
    }
  }
  
  for (uint8_t i = 0; i < 5; ++i) {
    if (gameState.asteroidsHard.bullets[i].on) {
      setPixel(gameState.asteroidsHard.bullets[i].x, gameState.asteroidsHard.bullets[i].y, Color(65, 65, 65));
    }
  }
  
  setPixel(gameState.asteroidsHard.shipX, 7, Color(0, 40, 45));
}

// ========================================
// PAC-MAN (EASY) GAME
// ========================================

void pacEasyRefillPellets() {
  gameState.pacEasy.pelletsLeft = 0;
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      gameState.pacEasy.pellets[y][x] = 1;
      gameState.pacEasy.pelletsLeft++;
    }
  }
  
  if (gameState.pacEasy.pellets[gameState.pacEasy.pacY][gameState.pacEasy.pacX]) {
    gameState.pacEasy.pellets[gameState.pacEasy.pacY][gameState.pacEasy.pacX] = 0;
    gameState.pacEasy.pelletsLeft--;
  }
}

void pacEasyInit() {
  gameState.pacEasy.pacX = 4;
  gameState.pacEasy.pacY = 4;
  gameState.pacEasy.direction = 0;
  gameState.pacEasy.ghostX = 0;
  gameState.pacEasy.ghostY = 0;
  gameState.pacEasy.lastPacMove = millis();
  gameState.pacEasy.lastGhostMove = millis();
  pacEasyRefillPellets();
}

void pacEasyUpdate(uint32_t now) {
  // Handle pause
  if (takePress(B_PAUSE)) {
    gamePaused = !gamePaused;
    beepButton();
    return;
  }
  
  if (gamePaused) return;
  
  if (takePress(B_UP)) gameState.pacEasy.direction = 3;
  else if (takePress(B_DOWN)) gameState.pacEasy.direction = 1;
  else if (takePress(B_LEFT)) gameState.pacEasy.direction = 2;
  else if (takePress(B_RIGHT)) gameState.pacEasy.direction = 0;
  
  if (now - gameState.pacEasy.lastPacMove > 250) {
    gameState.pacEasy.lastPacMove = now;
    
    int8_t newX = gameState.pacEasy.pacX;
    int8_t newY = gameState.pacEasy.pacY;
    
    switch (gameState.pacEasy.direction) {
      case 0: newX++; break;
      case 1: newY++; break;
      case 2: newX--; break;
      default: newY--; break;
    }
    
    if (newX < 0) newX = 7;
    if (newX >= 8) newX = 0;
    if (newY < 0) newY = 7;
    if (newY >= 8) newY = 0;
    
    gameState.pacEasy.pacX = newX;
    gameState.pacEasy.pacY = newY;
    
    if (gameState.pacEasy.pellets[gameState.pacEasy.pacY][gameState.pacEasy.pacX]) {
      gameState.pacEasy.pellets[gameState.pacEasy.pacY][gameState.pacEasy.pacX] = 0;
      gameState.pacEasy.pelletsLeft--;
      score += 10;
      beepScore();
      
      if (gameState.pacEasy.pelletsLeft == 0) {
        score += 100;
        pacEasyRefillPellets();
      }
    }
  }
  
  if (now - gameState.pacEasy.lastGhostMove > 400) {
    gameState.pacEasy.lastGhostMove = now;
    
    if (abs(gameState.pacEasy.ghostX - gameState.pacEasy.pacX) > 
        abs(gameState.pacEasy.ghostY - gameState.pacEasy.pacY)) {
      gameState.pacEasy.ghostX += (gameState.pacEasy.ghostX < gameState.pacEasy.pacX) ? 1 : -1;
    } else {
      gameState.pacEasy.ghostY += (gameState.pacEasy.ghostY < gameState.pacEasy.pacY) ? 1 : -1;
    }
    
    if (gameState.pacEasy.ghostX < 0) gameState.pacEasy.ghostX = 7;
    if (gameState.pacEasy.ghostX >= 8) gameState.pacEasy.ghostX = 0;
    if (gameState.pacEasy.ghostY < 0) gameState.pacEasy.ghostY = 7;
    if (gameState.pacEasy.ghostY >= 8) gameState.pacEasy.ghostY = 0;
  }
  
  if (gameState.pacEasy.pacX == gameState.pacEasy.ghostX && 
      gameState.pacEasy.pacY == gameState.pacEasy.ghostY) {
    beepHit();
    finishGame(false, "Caught");
  }
}

void pacEasyDraw() {
  clearFrame();
  
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      if (gameState.pacEasy.pellets[y][x]) {
        setPixel(static_cast<int8_t>(x), static_cast<int8_t>(y), Color(0, 28, 45));
      }
    }
  }
  
  setPixel(gameState.pacEasy.pacX, gameState.pacEasy.pacY, Color(130, 110, 0));
  setPixel(gameState.pacEasy.ghostX, gameState.pacEasy.ghostY, Color(65, 0, 0));
}

// ========================================
// PAC-MAN (HARD) GAME
// ========================================

const uint8_t PACMAN_WORLDS[4][8][8] = {
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

bool pacmanIsWall(int8_t x, int8_t y) {
  if (x < 0 || x > 7 || y < 0 || y > 7) return true;
  return gameState.pacman.cells[y][x] == 0;
}

void pacmanLoadWorld() {
  const uint8_t (*map)[8] = PACMAN_WORLDS[gameState.pacman.worldIndex];
  gameState.pacman.pelletsLeft = 0;
  
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      if (map[y][x] == 0) {
        gameState.pacman.cells[y][x] = 0;
      } else {
        gameState.pacman.cells[y][x] = 1;
        gameState.pacman.pelletsLeft++;
      }
    }
  }
  
  if (gameState.pacman.cells[gameState.pacman.pacY][gameState.pacman.pacX] == 1) {
    gameState.pacman.cells[gameState.pacman.pacY][gameState.pacman.pacX] = 2;
    gameState.pacman.pelletsLeft--;
  }
  
  if (gameState.pacman.cells[gameState.pacman.ghostY][gameState.pacman.ghostX] == 1) {
    gameState.pacman.cells[gameState.pacman.ghostY][gameState.pacman.ghostX] = 2;
    gameState.pacman.pelletsLeft--;
  }
}

void pacmanRespawn() {
  const uint8_t (*map)[8] = PACMAN_WORLDS[gameState.pacman.worldIndex];
  
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      if (map[y][x] != 0) {
        gameState.pacman.cells[y][x] = 1;
      }
    }
  }
  
  gameState.pacman.cells[gameState.pacman.pacY][gameState.pacman.pacX] = 2;
  gameState.pacman.cells[gameState.pacman.ghostY][gameState.pacman.ghostX] = 2;
  
  gameState.pacman.pelletsLeft = 0;
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      if (gameState.pacman.cells[y][x] == 1) {
        gameState.pacman.pelletsLeft++;
      }
    }
  }
}

void pacmanMoveGhost() {
  const int8_t directions[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  int8_t bestX = gameState.pacman.ghostX;
  int8_t bestY = gameState.pacman.ghostY;
  int8_t bestDist = 127;
  
  for (uint8_t i = 0; i < 4; ++i) {
    int8_t newX = static_cast<int8_t>(gameState.pacman.ghostX + directions[i][0]);
    int8_t newY = static_cast<int8_t>(gameState.pacman.ghostY + directions[i][1]);
    
    if (pacmanIsWall(newX, newY)) continue;
    
    int8_t dist = static_cast<int8_t>(abs(newX - gameState.pacman.pacX) + abs(newY - gameState.pacman.pacY));
    if (dist < bestDist || (dist == bestDist && random(0, 2) == 0)) {
      bestDist = dist;
      bestX = newX;
      bestY = newY;
    }
  }
  
  gameState.pacman.ghostX = bestX;
  gameState.pacman.ghostY = bestY;
}

void pacmanInit() {
  gameState.pacman.worldIndex = 0;
  gameState.pacman.pacX = 1;
  gameState.pacman.pacY = 1;
  gameState.pacman.ghostX = 6;
  gameState.pacman.ghostY = 6;
  gameState.pacman.dx = 1;
  gameState.pacman.dy = 0;
  gameState.pacman.nextDx = 1;
  gameState.pacman.nextDy = 0;
  gameState.pacman.pacInterval = 210;
  gameState.pacman.ghostInterval = 360;
  gameState.pacman.lastPacMove = millis();
  gameState.pacman.lastGhostMove = millis();
  pacmanLoadWorld();
}

void pacmanUpdate(uint32_t now) {
  // Handle pause
  if (takePress(B_PAUSE)) {
    gamePaused = !gamePaused;
    beepButton();
    return;
  }
  
  if (gamePaused) return;
  
  if (takePress(B_UP)) {
    gameState.pacman.nextDx = 0;
    gameState.pacman.nextDy = -1;
    beepButton();
  }
  if (takePress(B_DOWN)) {
    gameState.pacman.nextDx = 0;
    gameState.pacman.nextDy = 1;
    beepButton();
  }
  if (takePress(B_LEFT)) {
    gameState.pacman.nextDx = -1;
    gameState.pacman.nextDy = 0;
    beepButton();
  }
  if (takePress(B_RIGHT)) {
    gameState.pacman.nextDx = 1;
    gameState.pacman.nextDy = 0;
    beepButton();
  }
  
  if (now - gameState.pacman.lastPacMove >= gameState.pacman.pacInterval) {
    gameState.pacman.lastPacMove = now;
    
    if (!pacmanIsWall(static_cast<int8_t>(gameState.pacman.pacX + gameState.pacman.nextDx),
                      static_cast<int8_t>(gameState.pacman.pacY + gameState.pacman.nextDy))) {
      gameState.pacman.dx = gameState.pacman.nextDx;
      gameState.pacman.dy = gameState.pacman.nextDy;
    }
    
    int8_t newX = static_cast<int8_t>(gameState.pacman.pacX + gameState.pacman.dx);
    int8_t newY = static_cast<int8_t>(gameState.pacman.pacY + gameState.pacman.dy);
    
    if (!pacmanIsWall(newX, newY)) {
      gameState.pacman.pacX = newX;
      gameState.pacman.pacY = newY;
    }
    
    if (gameState.pacman.cells[gameState.pacman.pacY][gameState.pacman.pacX] == 1) {
      gameState.pacman.cells[gameState.pacman.pacY][gameState.pacman.pacX] = 2;
      if (gameState.pacman.pelletsLeft) gameState.pacman.pelletsLeft--;
      score++;
      beepScore();
    }
  }
  
  if (now - gameState.pacman.lastGhostMove >= gameState.pacman.ghostInterval) {
    gameState.pacman.lastGhostMove = now;
    pacmanMoveGhost();
  }
  
  if (gameState.pacman.pacX == gameState.pacman.ghostX && 
      gameState.pacman.pacY == gameState.pacman.ghostY) {
    beepHit();
    finishGame(false, "Caught");
    return;
  }
  
  if (gameState.pacman.pelletsLeft == 0) {
    if (gameState.pacman.worldIndex < 3) {
      gameState.pacman.worldIndex++;
      score += 25;
      beepScore();
      
      gameState.pacman.pacX = 1;
      gameState.pacman.pacY = 1;
      gameState.pacman.ghostX = 6;
      gameState.pacman.ghostY = 6;
      gameState.pacman.dx = 1;
      gameState.pacman.dy = 0;
      gameState.pacman.nextDx = 1;
      gameState.pacman.nextDy = 0;
      
      if (gameState.pacman.ghostInterval > 120) {
        gameState.pacman.ghostInterval = static_cast<uint16_t>(gameState.pacman.ghostInterval - 18);
      }
      if (gameState.pacman.pacInterval > 145) {
        gameState.pacman.pacInterval = static_cast<uint16_t>(gameState.pacman.pacInterval - 10);
      }
      
      pacmanRespawn();
    } else {
      score += 100;
      finishGame(true, "World 4 clear");
      return;
    }
  }
}

void pacmanDraw() {
  clearFrame();
  
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      if (gameState.pacman.cells[y][x] == 0) {
        setPixel(x, y, Color(0, 0, 24));
      } else if (gameState.pacman.cells[y][x] == 1) {
        setPixel(x, y, Color(170, 150, 55));
      }
    }
  }
  
  setPixel(gameState.pacman.ghostX, gameState.pacman.ghostY, Color(45, 0, 0));
  setPixel(gameState.pacman.pacX, gameState.pacman.pacY, Color(255, 95, 0));
}

// ========================================
// SPACE SHOOTER GAME
// ========================================

void spaceShooterInit() {
  gameState.shooter.shipX = 3;
  gameState.shooter.enemyInterval = 720;
  gameState.shooter.spawnInterval = 1350;
  gameState.shooter.lastMove = millis();
  gameState.shooter.lastPlayerBullet = millis();
  gameState.shooter.lastEnemyBullet = millis();
  gameState.shooter.lastEnemyStep = millis();
  gameState.shooter.lastSpawn = millis();
  gameState.shooter.lastShot = millis();
  gameState.shooter.lastEnemyShoot = millis();
  
  for (uint8_t i = 0; i < 6; ++i) {
    gameState.shooter.playerBullets[i].on = false;
  }
  for (uint8_t i = 0; i < 5; ++i) {
    gameState.shooter.enemyBullets[i].on = false;
  }
  for (uint8_t i = 0; i < 8; ++i) {
    gameState.shooter.enemies[i].active = false;
  }
}

void spaceShooterFirePlayer() {
  for (uint8_t i = 0; i < 6; ++i) {
    if (!gameState.shooter.playerBullets[i].on) {
      gameState.shooter.playerBullets[i] = {gameState.shooter.shipX, 6, true};
      return;
    }
  }
}

void spaceShooterSpawnEnemy() {
  for (uint8_t i = 0; i < 8; ++i) {
    if (!gameState.shooter.enemies[i].active) {
      gameState.shooter.enemies[i] = {static_cast<int8_t>(random(0, 8)), 0, true};
      return;
    }
  }
}

void spaceShooterFireEnemy(int8_t x, int8_t y) {
  for (uint8_t i = 0; i < 5; ++i) {
    if (!gameState.shooter.enemyBullets[i].on) {
      gameState.shooter.enemyBullets[i] = {x, y, true};
      return;
    }
  }
}

void spaceShooterUpdate(uint32_t now) {
  if (isDown(B_LEFT) && now - gameState.shooter.lastMove >= 125) {
    gameState.shooter.lastMove = now;
    if (gameState.shooter.shipX > 0) {
      gameState.shooter.shipX--;
      beepButton();
    }
  }
  
  if (isDown(B_RIGHT) && now - gameState.shooter.lastMove >= 125) {
    gameState.shooter.lastMove = now;
    if (gameState.shooter.shipX < 7) {
      gameState.shooter.shipX++;
      beepButton();
    }
  }
  
  if ((takePress(B_UP) || takePress(B_PAUSE)) && now - gameState.shooter.lastShot >= 320) {
    gameState.shooter.lastShot = now;
    spaceShooterFirePlayer();
    beepButton();
  }
  
  if (now - gameState.shooter.lastSpawn >= gameState.shooter.spawnInterval) {
    gameState.shooter.lastSpawn = now;
    spaceShooterSpawnEnemy();
  }
  
  if (now - gameState.shooter.lastPlayerBullet >= 130) {
    gameState.shooter.lastPlayerBullet = now;
    for (uint8_t i = 0; i < 6; ++i) {
      if (gameState.shooter.playerBullets[i].on) {
        gameState.shooter.playerBullets[i].y--;
        if (gameState.shooter.playerBullets[i].y < 0) {
          gameState.shooter.playerBullets[i].on = false;
        }
      }
    }
  }
  
  if (now - gameState.shooter.lastEnemyStep >= gameState.shooter.enemyInterval) {
    gameState.shooter.lastEnemyStep = now;
    for (uint8_t i = 0; i < 8; ++i) {
      if (gameState.shooter.enemies[i].active) {
        if (random(0, 3) == 0) {
          int8_t newX = static_cast<int8_t>(gameState.shooter.enemies[i].x + random(-1, 2));
          if (newX >= 0 && newX < 8) {
            gameState.shooter.enemies[i].x = newX;
          }
        }
        gameState.shooter.enemies[i].y++;
        if (gameState.shooter.enemies[i].y >= 7) {
          beepHit();
          finishGame(false, "Line broken");
          return;
        }
      }
    }
  }
  
  if (now - gameState.shooter.lastEnemyShoot >= 850) {
    gameState.shooter.lastEnemyShoot = now;
    uint8_t start = static_cast<uint8_t>(random(0, 8));
    for (uint8_t j = 0; j < 8; ++j) {
      uint8_t i = static_cast<uint8_t>((start + j) % 8);
      if (gameState.shooter.enemies[i].active) {
        spaceShooterFireEnemy(gameState.shooter.enemies[i].x, 
                             static_cast<int8_t>(gameState.shooter.enemies[i].y + 1));
        break;
      }
    }
  }
  
  if (now - gameState.shooter.lastEnemyBullet >= 190) {
    gameState.shooter.lastEnemyBullet = now;
    for (uint8_t i = 0; i < 5; ++i) {
      if (gameState.shooter.enemyBullets[i].on) {
        gameState.shooter.enemyBullets[i].y++;
        if (gameState.shooter.enemyBullets[i].y > 7) {
          gameState.shooter.enemyBullets[i].on = false;
        } else if (gameState.shooter.enemyBullets[i].y == 7 && 
                   gameState.shooter.enemyBullets[i].x == gameState.shooter.shipX) {
          beepHit();
          finishGame(false, "Shot down");
          return;
        }
      }
    }
  }
  
  for (uint8_t bi = 0; bi < 6; ++bi) {
    if (gameState.shooter.playerBullets[bi].on) {
      for (uint8_t ei = 0; ei < 8; ++ei) {
        if (gameState.shooter.enemies[ei].active &&
            gameState.shooter.playerBullets[bi].x == gameState.shooter.enemies[ei].x &&
            gameState.shooter.playerBullets[bi].y == gameState.shooter.enemies[ei].y) {
          gameState.shooter.playerBullets[bi].on = false;
          gameState.shooter.enemies[ei].active = false;
          score++;
          beepScore();
          break;
        }
      }
    }
  }
  
  int16_t newEnemyInterval = static_cast<int16_t>(720 - score * 2);
  if (newEnemyInterval < 300) newEnemyInterval = 300;
  gameState.shooter.enemyInterval = static_cast<uint16_t>(newEnemyInterval);
  
  int16_t newSpawnInterval = static_cast<int16_t>(1350 - score * 5);
  if (newSpawnInterval < 500) newSpawnInterval = 500;
  gameState.shooter.spawnInterval = static_cast<uint16_t>(newSpawnInterval);
}

void spaceShooterDraw() {
  clearFrame();
  
  for (uint8_t i = 0; i < 8; ++i) {
    if (gameState.shooter.enemies[i].active) {
      setPixel(gameState.shooter.enemies[i].x, gameState.shooter.enemies[i].y, Color(46, 0, 20));
    }
  }
  
  for (uint8_t i = 0; i < 6; ++i) {
    if (gameState.shooter.playerBullets[i].on) {
      setPixel(gameState.shooter.playerBullets[i].x, gameState.shooter.playerBullets[i].y, Color(65, 65, 65));
    }
  }
  
  for (uint8_t i = 0; i < 5; ++i) {
    if (gameState.shooter.enemyBullets[i].on) {
      setPixel(gameState.shooter.enemyBullets[i].x, gameState.shooter.enemyBullets[i].y, Color(55, 10, 0));
    }
  }
  
  setPixel(gameState.shooter.shipX, 7, Color(0, 60, 10));
}

// ========================================
// BREAKOUT GAME
// ========================================

void breakoutResetBricks() {
  for (uint8_t y = 0; y < 3; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      gameState.breakout.bricks[y][x] = true;
    }
  }
}

void breakoutResetBall() {
  gameState.breakout.ballX = 3;
  gameState.breakout.ballY = 6;
  gameState.breakout.velocityX = (random(0, 2) == 0) ? -1 : 1;
  gameState.breakout.velocityY = -1;
}

bool breakoutAnyBricks() {
  for (uint8_t y = 0; y < 3; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      if (gameState.breakout.bricks[y][x]) return true;
    }
  }
  return false;
}

void breakoutInit() {
  breakoutResetBricks();
  gameState.breakout.paddleX = 2;
  breakoutResetBall();
  gameState.breakout.moveInterval = 200;
  gameState.breakout.lastMove = millis();
  gameState.breakout.lastPaddleMove = millis();
}

void breakoutUpdate(uint32_t now) {
  // Handle pause
  if (takePress(B_PAUSE)) {
    gamePaused = !gamePaused;
    beepButton();
    return;
  }
  
  if (gamePaused) return;
  
  if (isDown(B_LEFT) && now - gameState.breakout.lastPaddleMove >= 80) {
    gameState.breakout.lastPaddleMove = now;
    if (gameState.breakout.paddleX > 0) {
      gameState.breakout.paddleX--;
      beepButton();
    }
  }
  
  if (isDown(B_RIGHT) && now - gameState.breakout.lastPaddleMove >= 80) {
    gameState.breakout.lastPaddleMove = now;
    if (gameState.breakout.paddleX < 5) {
      gameState.breakout.paddleX++;
      beepButton();
    }
  }
  
  if (now - gameState.breakout.lastMove < gameState.breakout.moveInterval) return;
  gameState.breakout.lastMove = now;
  
  int8_t newX = static_cast<int8_t>(gameState.breakout.ballX + gameState.breakout.velocityX);
  int8_t newY = static_cast<int8_t>(gameState.breakout.ballY + gameState.breakout.velocityY);
  
  if (newX < 0 || newX > 7) {
    gameState.breakout.velocityX = static_cast<int8_t>(-gameState.breakout.velocityX);
    newX = static_cast<int8_t>(gameState.breakout.ballX + gameState.breakout.velocityX);
  }
  
  if (newY < 0) {
    gameState.breakout.velocityY = 1;
    newY = static_cast<int8_t>(gameState.breakout.ballY + gameState.breakout.velocityY);
  }
  
  if (newY >= 0 && newY < 3 && gameState.breakout.bricks[newY][newX]) {
    gameState.breakout.bricks[newY][newX] = false;
    gameState.breakout.velocityY = static_cast<int8_t>(-gameState.breakout.velocityY);
    newY = static_cast<int8_t>(gameState.breakout.ballY + gameState.breakout.velocityY);
    score++;
    beepScore();
  }
  
  if (newY >= 7) {
    bool topHit = (newX >= gameState.breakout.paddleX && newX <= gameState.breakout.paddleX + 2);
    bool leftSideHit = (newX == gameState.breakout.paddleX - 1 && gameState.breakout.velocityX > 0);
    bool rightSideHit = (newX == gameState.breakout.paddleX + 3 && gameState.breakout.velocityX < 0);
    
    if (topHit || leftSideHit || rightSideHit) {
      gameState.breakout.velocityY = -1;
      if (leftSideHit || rightSideHit) {
        gameState.breakout.velocityX = static_cast<int8_t>(-gameState.breakout.velocityX);
      } else {
        int8_t hitPos = static_cast<int8_t>(newX - (gameState.breakout.paddleX + 1));
        if (hitPos < 0) gameState.breakout.velocityX = -1;
        if (hitPos > 0) gameState.breakout.velocityX = 1;
      }
      newX = static_cast<int8_t>(constrain(newX, 0, 7));
      newY = 6;
      beepButton();
    } else {
      beepHit();
      finishGame(false, "Missed ball");
      return;
    }
  }
  
  gameState.breakout.ballX = newX;
  gameState.breakout.ballY = newY;
  
  if (!breakoutAnyBricks()) {
    score += 5;
    beepWin();
    breakoutResetBricks();
    breakoutResetBall();
    
    if (gameState.breakout.moveInterval > 80) {
      gameState.breakout.moveInterval = static_cast<uint16_t>(gameState.breakout.moveInterval - 12);
    }
  }
}

void breakoutDraw() {
  clearFrame();
  
  for (uint8_t y = 0; y < 3; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      if (gameState.breakout.bricks[y][x]) {
        setPixel(x, y, Color(
          static_cast<uint8_t>(35 + y * 6),
          static_cast<uint8_t>(10 + y * 4),
          0
        ));
      }
    }
  }
  
  for (uint8_t x = 0; x < 3; ++x) {
    setPixel(static_cast<int8_t>(gameState.breakout.paddleX + x), 7, Color(0, 48, 50));
  }
  
  setPixel(gameState.breakout.ballX, gameState.breakout.ballY, Color(65, 65, 65));
}

// ========================================
// TIC-TAC-TOE GAME
// ========================================

void tttInit(bool aiMode) {
  memset(gameState.ttt.board, 0, sizeof(gameState.ttt.board));
  gameState.ttt.cursorX = 1;
  gameState.ttt.cursorY = 1;
  gameState.ttt.turn = 1;
  gameState.ttt.aiMode = aiMode;
  gameState.ttt.waitingForAI = false;
  gameState.ttt.aiMoveTime = 0;
}

uint8_t tttCheckWinner() {
  for (uint8_t i = 0; i < 3; ++i) {
    if (gameState.ttt.board[i][0] && 
        gameState.ttt.board[i][0] == gameState.ttt.board[i][1] && 
        gameState.ttt.board[i][1] == gameState.ttt.board[i][2]) {
      return gameState.ttt.board[i][0];
    }
    if (gameState.ttt.board[0][i] && 
        gameState.ttt.board[0][i] == gameState.ttt.board[1][i] && 
        gameState.ttt.board[1][i] == gameState.ttt.board[2][i]) {
      return gameState.ttt.board[0][i];
    }
  }
  
  if (gameState.ttt.board[0][0] && 
      gameState.ttt.board[0][0] == gameState.ttt.board[1][1] && 
      gameState.ttt.board[1][1] == gameState.ttt.board[2][2]) {
    return gameState.ttt.board[0][0];
  }
  if (gameState.ttt.board[0][2] && 
      gameState.ttt.board[0][2] == gameState.ttt.board[1][1] && 
      gameState.ttt.board[1][1] == gameState.ttt.board[2][0]) {
    return gameState.ttt.board[0][2];
  }
  
  for (uint8_t y = 0; y < 3; ++y) {
    for (uint8_t x = 0; x < 3; ++x) {
      if (!gameState.ttt.board[y][x]) return 0;
    }
  }
  
  return 3;  // Draw
}

bool tttFindWinningMove(uint8_t player, uint8_t &outX, uint8_t &outY) {
  for (uint8_t y = 0; y < 3; ++y) {
    for (uint8_t x = 0; x < 3; ++x) {
      if (!gameState.ttt.board[y][x]) {
        gameState.ttt.board[y][x] = player;
        bool wins = tttCheckWinner() == player;
        gameState.ttt.board[y][x] = 0;
        if (wins) {
          outX = x;
          outY = y;
          return true;
        }
      }
    }
  }
  return false;
}

void tttAIMove() {
  uint8_t x = 0, y = 0;
  
  if (tttFindWinningMove(2, x, y) || tttFindWinningMove(1, x, y)) {
    gameState.ttt.board[y][x] = 2;
    return;
  }
  
  if (!gameState.ttt.board[1][1]) {
    gameState.ttt.board[1][1] = 2;
    return;
  }
  
  const uint8_t priority[9][2] = {
    {0, 0}, {2, 0}, {0, 2}, {2, 2},
    {1, 0}, {0, 1}, {2, 1}, {1, 2}, {1, 1}
  };
  
  for (uint8_t i = 0; i < 9; ++i) {
    uint8_t px = priority[i][0];
    uint8_t py = priority[i][1];
    if (!gameState.ttt.board[py][px]) {
      gameState.ttt.board[py][px] = 2;
      return;
    }
  }
}

void tttFinish() {
  uint8_t result = tttCheckWinner();
  if (!result) return;
  
  if (result == 1) {
    score += 5;
    finishGame(true, gameState.ttt.aiMode ? "You win" : "X wins");
  } else if (result == 2) {
    finishGame(gameState.ttt.aiMode ? false : true, gameState.ttt.aiMode ? "AI wins" : "O wins");
  } else {
    finishGame(false, "Draw");
  }
}

void tttUpdate(uint32_t now) {
  if (gameState.ttt.aiMode && gameState.ttt.waitingForAI && now >= gameState.ttt.aiMoveTime) {
    gameState.ttt.waitingForAI = false;
    tttAIMove();
    tttFinish();
    if (systemState == STATE_PLAYING) {
      gameState.ttt.turn = 1;
    }
    return;
  }
  
  if (gameState.ttt.aiMode && gameState.ttt.turn == 2) return;
  
  if (takePress(B_UP) && gameState.ttt.cursorY > 0) {
    gameState.ttt.cursorY--;
    beepButton();
  }
  if (takePress(B_DOWN) && gameState.ttt.cursorY < 2) {
    gameState.ttt.cursorY++;
    beepButton();
  }
  if (takePress(B_LEFT) && gameState.ttt.cursorX > 0) {
    gameState.ttt.cursorX--;
    beepButton();
  }
  if (takePress(B_RIGHT) && gameState.ttt.cursorX < 2) {
    gameState.ttt.cursorX++;
    beepButton();
  }
  
  if (takePress(B_PAUSE)) {
    if (gameState.ttt.board[gameState.ttt.cursorY][gameState.ttt.cursorX]) {
      beepHit();
      return;
    }
    
    gameState.ttt.board[gameState.ttt.cursorY][gameState.ttt.cursorX] = gameState.ttt.turn;
    score++;
    beepButton();
    
    tttFinish();
    if (systemState != STATE_PLAYING) return;
    
    if (gameState.ttt.aiMode) {
      gameState.ttt.turn = 2;
      gameState.ttt.waitingForAI = true;
      gameState.ttt.aiMoveTime = now + 230;
    } else {
      gameState.ttt.turn = (gameState.ttt.turn == 1) ? 2 : 1;
    }
  }
}

void tttDraw() {
  clearFrame();
  
  const int8_t cellX[3] = {1, 3, 6};
  const int8_t cellY[3] = {1, 3, 6};
  
  for (uint8_t i = 0; i < 8; ++i) {
    setPixel(2, i, Color(8, 8, 8));
    setPixel(5, i, Color(8, 8, 8));
    setPixel(i, 2, Color(8, 8, 8));
    setPixel(i, 5, Color(8, 8, 8));
  }
  
  for (uint8_t y = 0; y < 3; ++y) {
    for (uint8_t x = 0; x < 3; ++x) {
      int8_t gridX = cellX[x];
      int8_t gridY = cellY[y];
      
      if (gameState.ttt.board[y][x] == 1) {
        setPixel(gridX, gridY, Color(0, 52, 0));
      } else if (gameState.ttt.board[y][x] == 2) {
        setPixel(gridX, gridY, Color(55, 0, 0));
      } else {
        setPixel(gridX, gridY, Color(10, 10, 10));
      }
      
      if (x == gameState.ttt.cursorX && y == gameState.ttt.cursorY && 
          (!gameState.ttt.aiMode || gameState.ttt.turn == 1)) {
        setPixel(gridX, gridY, gameState.ttt.board[y][x] ? Color(65, 65, 65) : Color(0, 0, 65));
      }
    }
  }
}

// ========================================
// AIR HOCKEY (CLEAN REWRITE)
// ========================================

void pongInit() {
  gameState.pong.playerX = 3;
  gameState.pong.aiX = 3;
  gameState.pong.paddleSize = 3;
  gameState.pong.ballX = 4;
  gameState.pong.ballY = 4;
  gameState.pong.velocityX = (random(0, 2) == 0) ? -1 : 1;
  gameState.pong.velocityY = 1;   // AI serves first: ball starts toward player side
  gameState.pong.moveInterval = 155;
  gameState.pong.lastMove = millis();
  gameState.pong.lastPaddleMove = millis();
  gameState.pong.lastAIMove = millis();
  score = 0;
}

void pongResetBall(int8_t directionY) {
  gameState.pong.ballX = 4;
  gameState.pong.ballY = 4;
  gameState.pong.velocityX = (random(0, 2) == 0) ? -1 : 1;
  gameState.pong.velocityY = directionY;
}

// ========================================
// UPDATE
// ========================================

void pongUpdate(uint32_t now) {
  if (takePress(B_PAUSE)) {
    gamePaused = !gamePaused;
    beepButton();
    return;
  }
  if (gamePaused) return;

  if (isDown(B_LEFT) && now - gameState.pong.lastPaddleMove >= 60) {
    gameState.pong.lastPaddleMove = now;
    if (gameState.pong.playerX > 0) {
      gameState.pong.playerX--;
    }
  }

  if (isDown(B_RIGHT) && now - gameState.pong.lastPaddleMove >= 60) {
    gameState.pong.lastPaddleMove = now;
    if (gameState.pong.playerX < 8 - gameState.pong.paddleSize) {
      gameState.pong.playerX++;
    }
  }
  
  if (now - gameState.pong.lastAIMove >= 110) {
    gameState.pong.lastAIMove = now;
    int8_t targetX = static_cast<int8_t>(gameState.pong.ballX - 1);
    targetX = static_cast<int8_t>(constrain(targetX, 0, 8 - gameState.pong.paddleSize));
    
    if (random(0, 100) < 85) {
      if (gameState.pong.aiX < targetX) {
        gameState.pong.aiX++;
      } else if (gameState.pong.aiX > targetX) {
        gameState.pong.aiX--;
      }
    }
  }
  
  if (now - gameState.pong.lastMove < gameState.pong.moveInterval) return;
  gameState.pong.lastMove = now;
  
  int8_t newX = gameState.pong.ballX + gameState.pong.velocityX;
  int8_t newY = gameState.pong.ballY + gameState.pong.velocityY;
  
  if (newX < 0) {
    newX = 0;
    gameState.pong.velocityX = 1;
  } else if (newX > 7) {
    newX = 7;
    gameState.pong.velocityX = -1;
  }
  
  if (newY >= 7 && gameState.pong.velocityY > 0) {
    if (newX >= gameState.pong.playerX &&
        newX < gameState.pong.playerX + gameState.pong.paddleSize) {
      newY = 6;
      gameState.pong.velocityY = -1;
      int8_t hit = newX - (gameState.pong.playerX + 1);
      if (hit < 0) {
        gameState.pong.velocityX = -1;
      } else if (hit > 0) {
        gameState.pong.velocityX = 1;
      }
      
      score++;
      beepButton();
    } else {
      beepHit();
      finishGame(false, "Missed paddle");
      return;
    }
  }
  
  if (newY <= 0 && gameState.pong.velocityY < 0) {
    if (newX >= gameState.pong.aiX &&
        newX < gameState.pong.aiX + gameState.pong.paddleSize) {
      newY = 1;
      gameState.pong.velocityY = 1;
      int8_t hit = newX - (gameState.pong.aiX + 1);
      if (hit < 0) {
        gameState.pong.velocityX = -1;
      } else if (hit > 0) {
        gameState.pong.velocityX = 1;
      }
      
      beepButton();
    } else {
      score += 20;
      beepScore();
      pongResetBall(1);
      // Keep acceleration gentle so the ball stays readable like breakout.
      if (gameState.pong.moveInterval > 120) {
        gameState.pong.moveInterval = static_cast<uint16_t>(gameState.pong.moveInterval - 1);
      }
      return;
    }
  }
  
  gameState.pong.ballX = constrain(newX, 0, 7);
  gameState.pong.ballY = constrain(newY, 0, 7);
}

// ========================================
// DRAW
// ========================================

void pongDraw() {
  clearFrame();

  // Player paddle (bottom)
  for (uint8_t i = 0; i < gameState.pong.paddleSize; i++) {
    setPixel(gameState.pong.playerX + i, 7, Color(0, 80, 0));
  }

  // AI paddle (top)
  for (uint8_t i = 0; i < gameState.pong.paddleSize; i++) {
    setPixel(gameState.pong.aiX + i, 0, Color(80, 0, 0));
  }

  // Ball
  setPixel(gameState.pong.ballX, gameState.pong.ballY, Color(80, 80, 80));
}

// ========================================
// TUG OF WAR GAME
// ========================================

void tugInit() {
  gameState.tug.position = 4;
  gameState.tug.player1LastPress = millis();
  gameState.tug.player2LastPress = millis();
  score = 0;
}

void tugUpdate(uint32_t now) {
  if ((takePress(B_UP) || takePress(B_LEFT)) && now - gameState.tug.player1LastPress >= 80) {
    gameState.tug.player1LastPress = now;
    gameState.tug.position--;
    score++;
    beepButton();
  }
  
  if ((takePress(B_DOWN) || takePress(B_RIGHT) || takePress(B_PAUSE)) && 
      now - gameState.tug.player2LastPress >= 80) {
    gameState.tug.player2LastPress = now;
    gameState.tug.position++;
    score++;
    beepButton();
  }
  
  if (gameState.tug.position <= 0) {
    finishGame(true, "P1 wins");
    return;
  }
  
  if (gameState.tug.position >= 7) {
    finishGame(true, "P2 wins");
  }
}

void tugDraw() {
  clearFrame();
  
  for (uint8_t x = 0; x < 8; ++x) {
    setPixel(x, 3, Color(8, 8, 8));
  }
  
  setPixel(0, 3, Color(0, 50, 0));
  setPixel(7, 3, Color(50, 0, 0));
  setPixel(gameState.tug.position, 3, Color(65, 55, 0));
}

// ========================================
// CHECKERS GAME
// ========================================
// 
// OFFICIAL RULES (American/English Checkers):
// 1. SETUP: 12 pieces per side on dark squares in first 3 rows
// 2. MOVEMENT (Regular pieces):
//    - Move one square diagonally FORWARD only
//    - CANNOT move backwards
// 3. JUMPING (Regular pieces in this implementation):
//    - Can jump over opponent diagonally FORWARD only
//    - Must jump if available (mandatory)
//    - Can make multiple jumps in one turn
//    - Jumped pieces are removed
// 4. KINGS:
//    - Promoted when reaching opposite end
//    - Can move AND jump in ALL diagonal directions
//    - Move one square at a time (not multiple squares)
// 5. WINNING:
//    - Capture all opponent pieces, OR
//    - Block opponent from making any legal moves
//
// ========================================

bool checkersIsPlaySquare(int8_t x, int8_t y) {
  // Only dark squares are playable in checkers
  return (x + y) % 2 == 1;
}

bool checkersIsPlayer1Piece(uint8_t piece) {
  return piece == 1 || piece == 2;
}

bool checkersIsPlayer2Piece(uint8_t piece) {
  return piece == 3 || piece == 4;
}

bool checkersIsKing(uint8_t piece) {
  return piece == 2 || piece == 4;
}

void checkersSetupBoard() {
  // Clear board
  memset(gameState.checkers.board, 0, sizeof(gameState.checkers.board));
  
  // Place player 2 pieces (top, rows 0-2)
  for (uint8_t y = 0; y < 3; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      if (checkersIsPlaySquare(x, y)) {
        gameState.checkers.board[y][x] = 3;  // Player 2 piece
      }
    }
  }
  
  // Place player 1 pieces (bottom, rows 5-7)
  for (uint8_t y = 5; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      if (checkersIsPlaySquare(x, y)) {
        gameState.checkers.board[y][x] = 1;  // Player 1 piece
      }
    }
  }
}

bool checkersCanJump(int8_t fromX, int8_t fromY, int8_t dx, int8_t dy, int8_t &toX, int8_t &toY) {
  uint8_t piece = gameState.checkers.board[fromY][fromX];
  if (!piece) return false;
  
  int8_t midX = fromX + dx;
  int8_t midY = fromY + dy;
  toX = fromX + dx * 2;
  toY = fromY + dy * 2;
  
  // Check bounds
  if (toX < 0 || toX >= 8 || toY < 0 || toY >= 8) return false;
  if (midX < 0 || midX >= 8 || midY < 0 || midY >= 8) return false;
  
  // Destination must be empty and playable
  if (gameState.checkers.board[toY][toX] != 0) return false;
  if (!checkersIsPlaySquare(toX, toY)) return false;
  
  uint8_t midPiece = gameState.checkers.board[midY][midX];
  
  // Must jump over opponent piece
  if (checkersIsPlayer1Piece(piece)) {
    return checkersIsPlayer2Piece(midPiece);
  } else {
    return checkersIsPlayer1Piece(midPiece);
  }
}

void checkersFindValidMoves() {
  gameState.checkers.validMoveCount = 0;
  
  if (gameState.checkers.selectedX < 0 || gameState.checkers.selectedY < 0) return;
  
  int8_t x = gameState.checkers.selectedX;
  int8_t y = gameState.checkers.selectedY;
  uint8_t piece = gameState.checkers.board[y][x];
  
  if (!piece) return;
  
  bool isP1 = checkersIsPlayer1Piece(piece);
  bool isKing = checkersIsKing(piece);
  
  // PHASE 1: Check capture moves
  // Non-king pieces may only capture forward to match the requested rules.
  int8_t jumpX, jumpY;
  bool canJump = false;
  
  if ((isKing || isP1) && checkersCanJump(x, y, -1, -1, jumpX, jumpY)) {
    gameState.checkers.validMoves[gameState.checkers.validMoveCount][0] = jumpX;
    gameState.checkers.validMoves[gameState.checkers.validMoveCount][1] = jumpY;
    gameState.checkers.validMoveCount++;
    canJump = true;
  }
  if ((isKing || isP1) && checkersCanJump(x, y, 1, -1, jumpX, jumpY)) {
    gameState.checkers.validMoves[gameState.checkers.validMoveCount][0] = jumpX;
    gameState.checkers.validMoves[gameState.checkers.validMoveCount][1] = jumpY;
    gameState.checkers.validMoveCount++;
    canJump = true;
  }
  if ((isKing || !isP1) && checkersCanJump(x, y, -1, 1, jumpX, jumpY)) {
    gameState.checkers.validMoves[gameState.checkers.validMoveCount][0] = jumpX;
    gameState.checkers.validMoves[gameState.checkers.validMoveCount][1] = jumpY;
    gameState.checkers.validMoveCount++;
    canJump = true;
  }
  if ((isKing || !isP1) && checkersCanJump(x, y, 1, 1, jumpX, jumpY)) {
    gameState.checkers.validMoves[gameState.checkers.validMoveCount][0] = jumpX;
    gameState.checkers.validMoves[gameState.checkers.validMoveCount][1] = jumpY;
    gameState.checkers.validMoveCount++;
    canJump = true;
  }
  
  // If we found jumps, ONLY return jumps (captures are mandatory)
  if (canJump) return;
  
  // PHASE 2: Check REGULAR moves (non-captures)
  // Regular pieces can ONLY move forward, kings can move any direction
  const int8_t dirs[4][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
  
  for (uint8_t i = 0; i < 4; ++i) {
    int8_t dx = dirs[i][0];
    int8_t dy = dirs[i][1];
    
    // *** FIX: Regular pieces can ONLY move forward (not jump, just move) ***
    if (!isKing) {
      // Player 1 (green, bottom) moves UP (negative dy)
      if (isP1 && dy > 0) continue;
      
      // Player 2 (red, top) moves DOWN (positive dy)
      if (!isP1 && dy < 0) continue;
    }
    
    int8_t newX = x + dx;
    int8_t newY = y + dy;
    
    // Check bounds
    if (newX < 0 || newX >= 8 || newY < 0 || newY >= 8) continue;
    
    // Only dark squares are playable
    if (!checkersIsPlaySquare(newX, newY)) continue;
    
    // Destination must be empty
    if (gameState.checkers.board[newY][newX] != 0) continue;
    
    // Valid move!
    gameState.checkers.validMoves[gameState.checkers.validMoveCount][0] = newX;
    gameState.checkers.validMoves[gameState.checkers.validMoveCount][1] = newY;
    gameState.checkers.validMoveCount++;
  }
}

bool checkersIsValidMove(int8_t toX, int8_t toY) {
  for (uint8_t i = 0; i < gameState.checkers.validMoveCount; ++i) {
    if (gameState.checkers.validMoves[i][0] == toX && 
        gameState.checkers.validMoves[i][1] == toY) {
      return true;
    }
  }
  return false;
}

void checkersExecuteMove(int8_t toX, int8_t toY) {
  int8_t fromX = gameState.checkers.selectedX;
  int8_t fromY = gameState.checkers.selectedY;
  
  uint8_t piece = gameState.checkers.board[fromY][fromX];
  
  // Check if this is a capture
  int8_t dx = toX - fromX;
  int8_t dy = toY - fromY;
  
  if (abs(dx) == 2) {
    // Remove captured piece
    int8_t capX = fromX + dx / 2;
    int8_t capY = fromY + dy / 2;
    gameState.checkers.board[capY][capX] = 0;
    score += 10;
    beepScore();
  }
  
  // Move piece
  gameState.checkers.board[toY][toX] = piece;
  gameState.checkers.board[fromY][fromX] = 0;
  
  // Check for king promotion
  if (piece == 1 && toY == 0) {
    gameState.checkers.board[toY][toX] = 2;  // P1 becomes king
    beepWin();
  } else if (piece == 3 && toY == 7) {
    gameState.checkers.board[toY][toX] = 4;  // P2 becomes king
    beepWin();
  }
  
  // Deselect piece
  gameState.checkers.selectedX = -1;
  gameState.checkers.selectedY = -1;
  gameState.checkers.validMoveCount = 0;
  
  // Switch players
  gameState.checkers.currentPlayer = (gameState.checkers.currentPlayer == 1) ? 2 : 1;
  beepButton();
}

bool checkersHasAnyMoves(uint8_t player) {
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      uint8_t piece = gameState.checkers.board[y][x];
      if (!piece) continue;
      
      bool isPlayerPiece = (player == 1) ? checkersIsPlayer1Piece(piece) : checkersIsPlayer2Piece(piece);
      if (!isPlayerPiece) continue;
      
      // Save current selection
      int8_t oldSelX = gameState.checkers.selectedX;
      int8_t oldSelY = gameState.checkers.selectedY;
      
      // Temporarily select this piece
      gameState.checkers.selectedX = x;
      gameState.checkers.selectedY = y;
      checkersFindValidMoves();
      
      // Restore selection
      gameState.checkers.selectedX = oldSelX;
      gameState.checkers.selectedY = oldSelY;
      
      if (gameState.checkers.validMoveCount > 0) {
        checkersFindValidMoves();  // Restore valid moves for actual selection
        return true;
      }
    }
  }
  checkersFindValidMoves();  // Restore valid moves for actual selection
  return false;
}

bool checkersCheckGameOver() {
  // Count pieces
  uint8_t p1Count = 0, p2Count = 0;
  
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      uint8_t piece = gameState.checkers.board[y][x];
      if (checkersIsPlayer1Piece(piece)) p1Count++;
      if (checkersIsPlayer2Piece(piece)) p2Count++;
    }
  }
  
  // Check if either player has no pieces
  if (p1Count == 0) {
    finishGame(false, "Player 2 wins");
    return true;
  }
  if (p2Count == 0) {
    finishGame(true, "Player 1 wins");
    return true;
  }
  
  // Check if current player has no moves
  if (!checkersHasAnyMoves(gameState.checkers.currentPlayer)) {
    if (gameState.checkers.currentPlayer == 1) {
      finishGame(false, "P2 wins (no moves)");
    } else {
      finishGame(true, "P1 wins (no moves)");
    }
    return true;
  }
  
  return false;
}

// ========================================
// CHECKERS AI
// ========================================

struct CheckersMove {
  int8_t fromX;
  int8_t fromY;
  int8_t toX;
  int8_t toY;
  int16_t score;
};

int16_t checkersEvaluateMove(int8_t fromX, int8_t fromY, int8_t toX, int8_t toY) {
  int16_t score = 0;
  
  uint8_t piece = gameState.checkers.board[fromY][fromX];
  bool isKing = checkersIsKing(piece);
  
  // Check if this is a capture
  int8_t dx = toX - fromX;
  int8_t dy = toY - fromY;
  bool isCapture = (abs(dx) == 2);
  
  if (isCapture) {
    // Captures are highly valuable
    score += 100;
    
    // Check if capturing a king
    int8_t capX = fromX + dx / 2;
    int8_t capY = fromY + dy / 2;
    if (checkersIsKing(gameState.checkers.board[capY][capX])) {
      score += 50;  // Capturing a king is even better
    }
  }
  
  // King promotion is very valuable
  if (!isKing && toY == 7) {
    score += 80;
  }
  
  // Moving toward promotion is good
  if (!isKing) {
    score += toY * 5;  // Closer to promotion (row 7 for AI pieces) = higher score
  }
  
  // Central control is valuable
  int8_t centerDist = abs(toX - 3) + abs(toX - 4);
  score += (8 - centerDist) * 3;
  
  // Edge squares are less desirable (easier to trap)
  if (toX == 0 || toX == 7) score -= 10;
  
  // Check if move leaves piece vulnerable
  // (simplified: check if moving to a square where opponent can capture)
  int8_t attackDirs[4][2] = {{-1, 1}, {1, 1}, {-1, -1}, {1, -1}};
  for (uint8_t i = 0; i < 4; ++i) {
    int8_t checkX = toX + attackDirs[i][0];
    int8_t checkY = toY + attackDirs[i][1];
    if (checkX >= 0 && checkX < 8 && checkY >= 0 && checkY < 8) {
      uint8_t adjacentPiece = gameState.checkers.board[checkY][checkX];
      if (checkersIsPlayer1Piece(adjacentPiece)) {
        // Enemy piece diagonal - check if we'd be capturable
        int8_t behindX = toX - attackDirs[i][0];
        int8_t behindY = toY - attackDirs[i][1];
        if (behindX >= 0 && behindX < 8 && behindY >= 0 && behindY < 8) {
          if (gameState.checkers.board[behindY][behindX] == 0) {
            score -= 25;  // Vulnerable position
          }
        }
      }
    }
  }
  
  // Add small random factor to avoid predictability
  score += random(-5, 6);
  
  return score;
}

void checkersAIMove(uint32_t now) {
  CheckersMove bestMove = {-1, -1, -1, -1, -32000};
  
  // Find all possible moves for AI (Player 2)
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      uint8_t piece = gameState.checkers.board[y][x];
      if (!checkersIsPlayer2Piece(piece)) continue;
      
      // Temporarily select this piece to find its moves
      int8_t oldSelX = gameState.checkers.selectedX;
      int8_t oldSelY = gameState.checkers.selectedY;
      uint8_t oldMoveCount = gameState.checkers.validMoveCount;
      
      gameState.checkers.selectedX = x;
      gameState.checkers.selectedY = y;
      checkersFindValidMoves();
      
      // Evaluate each valid move
      for (uint8_t i = 0; i < gameState.checkers.validMoveCount; ++i) {
        int8_t toX = gameState.checkers.validMoves[i][0];
        int8_t toY = gameState.checkers.validMoves[i][1];
        
        int16_t score = checkersEvaluateMove(x, y, toX, toY);
        
        if (score > bestMove.score) {
          bestMove.fromX = x;
          bestMove.fromY = y;
          bestMove.toX = toX;
          bestMove.toY = toY;
          bestMove.score = score;
        }
      }
      
      // Restore selection
      gameState.checkers.selectedX = oldSelX;
      gameState.checkers.selectedY = oldSelY;
      gameState.checkers.validMoveCount = oldMoveCount;
    }
  }
  
  // If we found a valid move, execute it
  if (bestMove.fromX >= 0) {
    // Select the piece
    gameState.checkers.selectedX = bestMove.fromX;
    gameState.checkers.selectedY = bestMove.fromY;
    checkersFindValidMoves();
    
    // Execute the move
    checkersExecuteMove(bestMove.toX, bestMove.toY);
    
    // Keep AI move visible for a short blink window.
    gameState.checkers.aiLastFromX = bestMove.fromX;
    gameState.checkers.aiLastFromY = bestMove.fromY;
    gameState.checkers.aiLastToX = bestMove.toX;
    gameState.checkers.aiLastToY = bestMove.toY;
    gameState.checkers.aiBlinkVisible = true;
    gameState.checkers.aiBlinkNextToggleAt = now + 140;
    gameState.checkers.aiBlinkEndsAt = now + 1100;
  }
}

void checkersInit(bool aiMode) {
  checkersSetupBoard();
  gameState.checkers.cursorX = 0;
  gameState.checkers.cursorY = 5;
  gameState.checkers.selectedX = -1;
  gameState.checkers.selectedY = -1;
  gameState.checkers.currentPlayer = 1;
  gameState.checkers.mustCapture = false;
  gameState.checkers.validMoveCount = 0;
  gameState.checkers.aiMode = aiMode;
  gameState.checkers.waitingForAI = false;
  gameState.checkers.aiMoveTime = 0;
  gameState.checkers.aiLastFromX = -1;
  gameState.checkers.aiLastFromY = -1;
  gameState.checkers.aiLastToX = -1;
  gameState.checkers.aiLastToY = -1;
  gameState.checkers.aiBlinkVisible = false;
  gameState.checkers.aiBlinkNextToggleAt = 0;
  gameState.checkers.aiBlinkEndsAt = 0;
  score = 0;
}

void checkersUpdate(uint32_t now) {
  if (gameState.checkers.aiMode && gameState.checkers.aiBlinkEndsAt > 0) {
    if (now >= gameState.checkers.aiBlinkEndsAt) {
      gameState.checkers.aiBlinkEndsAt = 0;
      gameState.checkers.aiBlinkVisible = false;
    } else {
      if (now >= gameState.checkers.aiBlinkNextToggleAt) {
        gameState.checkers.aiBlinkVisible = !gameState.checkers.aiBlinkVisible;
        gameState.checkers.aiBlinkNextToggleAt = now + 140;
      }
      return;
    }
  }
  
  // Handle AI turn
  if (gameState.checkers.aiMode && gameState.checkers.currentPlayer == 2) {
    if (!gameState.checkers.waitingForAI) {
      // Start AI thinking
      gameState.checkers.waitingForAI = true;
      gameState.checkers.aiMoveTime = now + 500;  // AI thinks for 0.5 seconds
      return;
    }
    
    if (now >= gameState.checkers.aiMoveTime) {
      gameState.checkers.waitingForAI = false;
      checkersAIMove(now);
      checkersCheckGameOver();
      return;
    }
    
    // Still waiting for AI
    return;
  }
  
  // Human player controls
  if (takePress(B_UP) && gameState.checkers.cursorY > 0) {
    gameState.checkers.cursorY--;
    beepButton();
  }
  if (takePress(B_DOWN) && gameState.checkers.cursorY < 7) {
    gameState.checkers.cursorY++;
    beepButton();
  }
  if (takePress(B_LEFT) && gameState.checkers.cursorX > 0) {
    gameState.checkers.cursorX--;
    beepButton();
  }
  if (takePress(B_RIGHT) && gameState.checkers.cursorX < 7) {
    gameState.checkers.cursorX++;
    beepButton();
  }
  
  if (takePress(B_PAUSE)) {
    int8_t x = gameState.checkers.cursorX;
    int8_t y = gameState.checkers.cursorY;
    
    // If no piece selected, try to select one
    if (gameState.checkers.selectedX < 0) {
      uint8_t piece = gameState.checkers.board[y][x];
      
      // Check if this is current player's piece
      bool validSelection = false;
      if (gameState.checkers.currentPlayer == 1 && checkersIsPlayer1Piece(piece)) {
        validSelection = true;
      } else if (gameState.checkers.currentPlayer == 2 && checkersIsPlayer2Piece(piece)) {
        validSelection = true;
      }
      
      if (validSelection) {
        gameState.checkers.selectedX = x;
        gameState.checkers.selectedY = y;
        checkersFindValidMoves();
        
        if (gameState.checkers.validMoveCount > 0) {
          beepButton();
        } else {
          // No valid moves, deselect
          gameState.checkers.selectedX = -1;
          gameState.checkers.selectedY = -1;
          beepHit();
        }
      } else {
        beepHit();
      }
    } else {
      // Piece is selected, try to move or deselect
      if (x == gameState.checkers.selectedX && y == gameState.checkers.selectedY) {
        // Clicked same piece, deselect
        gameState.checkers.selectedX = -1;
        gameState.checkers.selectedY = -1;
        gameState.checkers.validMoveCount = 0;
        beepButton();
      } else if (checkersIsValidMove(x, y)) {
        // Valid move
        checkersExecuteMove(x, y);
        checkersCheckGameOver();
      } else {
        beepHit();
      }
    }
  }
}

void checkersDraw() {
  clearFrame();
  
  // Draw checkerboard pattern and pieces
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      uint32_t squareColor;
      
      // Checkerboard pattern
      if (checkersIsPlaySquare(x, y)) {
        squareColor = Color(15, 15, 15);  // Dark squares
      } else {
        squareColor = Color(5, 5, 5);     // Light squares
        setPixel(x, y, squareColor);
        continue;
      }
      
      uint8_t piece = gameState.checkers.board[y][x];
      
      if (piece == 0) {
        // Empty dark square
        setPixel(x, y, squareColor);
      } else if (checkersIsPlayer1Piece(piece)) {
        // Player 1 piece (green)
        if (checkersIsKing(piece)) {
          setPixel(x, y, Color(50, 200, 50));  // MUCH lighter green king
        } else {
          setPixel(x, y, Color(0, 70, 0));     // Dark green regular
        }
      } else {
        // Player 2 piece (red)
        if (checkersIsKing(piece)) {
          setPixel(x, y, Color(200, 50, 50));  // MUCH lighter red king
        } else {
          setPixel(x, y, Color(70, 0, 0));     // Dark red regular
        }
      }
    }
  }
  
  // Show AI thinking indicator
  if (gameState.checkers.aiMode && gameState.checkers.waitingForAI) {
    // Pulsing red border to indicate AI is thinking
    bool pulse = ((millis() / 200) % 2) == 0;
    if (pulse) {
      for (uint8_t i = 0; i < 8; ++i) {
        setPixel(i, 0, Color(40, 0, 0));
        setPixel(i, 7, Color(40, 0, 0));
        setPixel(0, i, Color(40, 0, 0));
        setPixel(7, i, Color(40, 0, 0));
      }
    }
    return;  // Don't show cursor or highlights while AI is thinking
  }
  
  if (gameState.checkers.aiMode && gameState.checkers.aiBlinkEndsAt > 0) {
    if (gameState.checkers.aiBlinkVisible) {
      setPixel(gameState.checkers.aiLastFromX, gameState.checkers.aiLastFromY, Color(95, 95, 0));
      setPixel(gameState.checkers.aiLastToX, gameState.checkers.aiLastToY, Color(0, 95, 95));
    }
    return;
  }
  
  // Highlight valid moves
  for (uint8_t i = 0; i < gameState.checkers.validMoveCount; ++i) {
    int8_t mx = gameState.checkers.validMoves[i][0];
    int8_t my = gameState.checkers.validMoves[i][1];
    
    // Yellow highlight for valid moves
    setPixel(mx, my, Color(50, 50, 0));
  }
  
  // Highlight selected piece
  if (gameState.checkers.selectedX >= 0 && gameState.checkers.selectedY >= 0) {
    int8_t sx = gameState.checkers.selectedX;
    int8_t sy = gameState.checkers.selectedY;
    
    // Bright yellow for selected piece
    setPixel(sx, sy, Color(100, 100, 0));
  }
  
  // Draw cursor (only for human player's turn)
  if (!gameState.checkers.aiMode || gameState.checkers.currentPlayer == 1) {
    int8_t cx = gameState.checkers.cursorX;
    int8_t cy = gameState.checkers.cursorY;
    
    // Pulsing cursor
    bool pulse = ((millis() / 300) % 2) == 0;
    if (pulse) {
      setPixel(cx, cy, Color(80, 80, 80));
    }
  }
}

// ========================================
// Minesweeper Game
// ========================================

void minesweeperInit() {
  memset(&gameState.minesweeper, 0, sizeof(Minesweeper));
  gameState.minesweeper.cursorX = 4;
  gameState.minesweeper.cursorY = 4;
  gameState.minesweeper.minesLeft = 10;  // 10 mines on 8x8 board
  gameState.minesweeper.cellsToReveal = 64 - 10;  // All cells except mines
  gameState.minesweeper.firstClick = true;
  score = 0;
  
  // All cells start hidden
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      gameState.minesweeper.revealed[y][x] = 0;  // Hidden
      gameState.minesweeper.board[y][x] = 0;     // Will be set after first click
    }
  }
}

void minesweeperPlaceMines(int8_t safeX, int8_t safeY) {
  // Place 10 mines randomly, but not on the first clicked cell
  uint8_t minesPlaced = 0;
  
  while (minesPlaced < 10) {
    int8_t x = random(8);
    int8_t y = random(8);
    
    // Skip if already a mine or the safe cell
    if (gameState.minesweeper.board[y][x] == 9 || (x == safeX && y == safeY)) {
      continue;
    }
    
    gameState.minesweeper.board[y][x] = 9;  // 9 = mine
    minesPlaced++;
  }
  
  // Calculate numbers for non-mine cells
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      if (gameState.minesweeper.board[y][x] == 9) continue;  // Skip mines
      
      uint8_t count = 0;
      // Check all 8 adjacent cells
      for (int8_t dy = -1; dy <= 1; ++dy) {
        for (int8_t dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0) continue;
          int8_t nx = x + dx;
          int8_t ny = y + dy;
          if (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
            if (gameState.minesweeper.board[ny][nx] == 9) count++;
          }
        }
      }
      gameState.minesweeper.board[y][x] = count;
    }
  }
}

void minesweeperFloodFill(int8_t x, int8_t y) {
  // Reveal empty cells recursively (flood fill)
  if (x < 0 || x >= 8 || y < 0 || y >= 8) return;
  if (gameState.minesweeper.revealed[y][x] != 0) return;  // Already revealed or flagged
  
  gameState.minesweeper.revealed[y][x] = 1;  // Reveal
  gameState.minesweeper.cellsToReveal--;
  score++;
  
  // If this cell has no adjacent mines, reveal neighbors
  if (gameState.minesweeper.board[y][x] == 0) {
    for (int8_t dy = -1; dy <= 1; ++dy) {
      for (int8_t dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) continue;
        minesweeperFloodFill(x + dx, y + dy);
      }
    }
  }
}

void minesweeperUpdate(uint32_t now) {
  int8_t cx = gameState.minesweeper.cursorX;
  int8_t cy = gameState.minesweeper.cursorY;
  
  // Move cursor with UP/DOWN/LEFT/RIGHT
  if (takePress(B_UP)) {
    gameState.minesweeper.cursorY = (gameState.minesweeper.cursorY + 7) % 8;
    beepButton();
  }
  if (takePress(B_DOWN)) {
    gameState.minesweeper.cursorY = (gameState.minesweeper.cursorY + 1) % 8;
    beepButton();
  }
  if (takePress(B_LEFT)) {
    gameState.minesweeper.cursorX = (gameState.minesweeper.cursorX + 7) % 8;
    beepButton();
  }
  if (takePress(B_RIGHT)) {
    gameState.minesweeper.cursorX = (gameState.minesweeper.cursorX + 1) % 8;
    beepButton();
  }
  
  // Update cursor position after movement
  cx = gameState.minesweeper.cursorX;
  cy = gameState.minesweeper.cursorY;
  
  // PAUSE button handling with long press detection
  static uint32_t pausePressStart = 0;
  static bool pauseWasPressed = false;
  static const uint32_t LONG_PRESS_MS = 500;  // 500ms = long press
  
  if (isDown(B_PAUSE)) {
    if (!pauseWasPressed) {
      // Just started pressing
      pausePressStart = now;
      pauseWasPressed = true;
    } else if (now - pausePressStart >= LONG_PRESS_MS) {
      // LONG PRESS = Reveal cell (only trigger once)
      if (gameState.minesweeper.revealed[cy][cx] == 0) {  // Hidden cell only
        // First click - place mines avoiding this cell
        if (gameState.minesweeper.firstClick) {
          minesweeperPlaceMines(cx, cy);
          gameState.minesweeper.firstClick = false;
        }
        
        // Check if mine
        if (gameState.minesweeper.board[cy][cx] == 9) {
          // Hit a mine - game over
          
          // Reveal all mines
          for (uint8_t y = 0; y < 8; ++y) {
            for (uint8_t x = 0; x < 8; ++x) {
              if (gameState.minesweeper.board[y][x] == 9) {
                gameState.minesweeper.revealed[y][x] = 1;
              }
            }
          }
          
          finishGame(false, "Hit a mine!");
        } else {
          // Safe cell - reveal it (and neighbors if empty)
          beepScore();
          minesweeperFloodFill(cx, cy);
          
          // Check win condition
          if (gameState.minesweeper.cellsToReveal == 0) {
            finishGame(true, "All clear!");
          }
        }
      }
      pauseWasPressed = false;  // Reset to prevent repeat
    }
  } else if (pauseWasPressed) {
    // Button released - check if it was a short press
    if (now - pausePressStart < LONG_PRESS_MS) {
      // SHORT PRESS = Toggle flag
      if (gameState.minesweeper.revealed[cy][cx] == 0) {
        // Hidden - add flag
        beepButton();
        if (gameState.minesweeper.minesLeft > 0) {
          gameState.minesweeper.revealed[cy][cx] = 2;  // Flag
          gameState.minesweeper.minesLeft--;
        }
      } else if (gameState.minesweeper.revealed[cy][cx] == 2) {
        // Flagged - remove flag
        beepButton();
        gameState.minesweeper.revealed[cy][cx] = 0;  // Unflag
        gameState.minesweeper.minesLeft++;
      }
    }
    pauseWasPressed = false;
  }
}

void minesweeperDraw() {
  clearFrame();
  
  for (uint8_t y = 0; y < 8; ++y) {
    for (uint8_t x = 0; x < 8; ++x) {
      uint8_t state = gameState.minesweeper.revealed[y][x];
      
      if (state == 0) {
        // Hidden cell - gray
        setPixel(x, y, Color(15, 15, 15));
      } else if (state == 2) {
        // Flagged cell - blue
        setPixel(x, y, Color(0, 0, 90));
      } else {
        // Revealed cell
        uint8_t val = gameState.minesweeper.board[y][x];
        if (val == 9) {
          // Mine - bright red
          setPixel(x, y, Color(100, 0, 0));
        } else if (val == 0) {
          // Empty safe zone - light purple/magenta
          setPixel(x, y, Color(40, 0, 40));
        } else {
          // Number - color coded by danger
          if (val == 1) setPixel(x, y, Color(0, 50, 0));       // Green
          else if (val == 2) setPixel(x, y, Color(50, 50, 0)); // Yellow
          else if (val == 3) setPixel(x, y, Color(70, 35, 0)); // Orange
          else setPixel(x, y, Color(val * 15, 0, 0));          // Red
        }
      }
    }
  }
  
  // Draw cursor
  bool pulse = ((millis() / 250) % 2) == 0;
  if (pulse) {
    int8_t cx = gameState.minesweeper.cursorX;
    int8_t cy = gameState.minesweeper.cursorY;
    setPixel(cx, cy, Color(100, 100, 100));
  }
}

// ========================================
// Dino Run Game
// ========================================

void dinoNewObstacle() {
  gameState.dino.obstacleType = static_cast<uint8_t>(random(0, 2)); // 0=ground, 1=flying
  gameState.dino.obstacleW = static_cast<uint8_t>(random(1, 3)); // 1 or 2 wide
  gameState.dino.obstacleX = -gameState.dino.obstacleW; // Start off-screen left

  if (gameState.dino.obstacleType == 0) { // Ground obstacle (cactus)
    gameState.dino.obstacleH = static_cast<uint8_t>(random(1, 4)); // 1, 2, or 3 high
    gameState.dino.obstacleY = 7; // Sits on the ground
  } else { // Flying obstacle
    gameState.dino.obstacleH = static_cast<uint8_t>(random(1, 3)); // 1 or 2 high
    gameState.dino.obstacleY = static_cast<int8_t>(random(4, 6)); // y-pos 4 or 5
  }
  gameState.dino.passedObstacle = false;
}

void dinoInit() {
  gameState.dino.playerY8 = 7 * 8; // Start on the ground (y=7)
  gameState.dino.velocityY8 = 0;
  gameState.dino.isCrouching = false;
  gameState.dino.moveInterval = 150; // Initial speed
  gameState.dino.lastMove = millis();
  dinoNewObstacle();
  score = 0;
}

void dinoUpdate(uint32_t now) {
  if (takePress(B_PAUSE)) {
    gamePaused = !gamePaused;
    beepButton();
  }
  if (gamePaused) return;

  const int8_t DINO_X_POS = 6;
  int8_t playerY = static_cast<int8_t>(gameState.dino.playerY8 / 8);

  // Handle crouching
  if (isDown(B_DOWN) && playerY == 7) {
    gameState.dino.isCrouching = true;
  } else {
    gameState.dino.isCrouching = false;
  }

  // JUMP: only if on the ground
  if (takePress(B_UP) && playerY == 7 && !gameState.dino.isCrouching) {
    float speedFactor = 150.0f / gameState.dino.moveInterval;
    gameState.dino.velocityY8 = static_cast<int16_t>(-28.0f * speedFactor); // Increased jump speed
    beepButton();
  }

  // Fast fall (spamming crouch in air)
  if (takePress(B_DOWN) && playerY < 7) {
    gameState.dino.velocityY8 += 30;
  }

  if (now - gameState.dino.lastMove < gameState.dino.moveInterval) return;
  gameState.dino.lastMove = now;

  float speedFactor = 150.0f / gameState.dino.moveInterval;

  // Apply gravity
  gameState.dino.velocityY8 += static_cast<int16_t>(6.0f * speedFactor); // Increased fall speed (gravity)
  if (gameState.dino.velocityY8 > 24) {
    gameState.dino.velocityY8 = 24; // Terminal velocity
  }

  // Update player position
  gameState.dino.playerY8 += gameState.dino.velocityY8;

  // Check for ground collision
  if (gameState.dino.playerY8 >= 7 * 8) {
    gameState.dino.playerY8 = 7 * 8;
    gameState.dino.velocityY8 = 0;
  }

  // Move obstacle
  gameState.dino.obstacleX++;

  // Score obstacle
  if (!gameState.dino.passedObstacle && gameState.dino.obstacleX > DINO_X_POS) {
    gameState.dino.passedObstacle = true;
    score++;
    beepScore();
    // Increase speed
    if (gameState.dino.moveInterval > 60) {
      gameState.dino.moveInterval -= 4;
    }
  }

  // Spawn new obstacle when old one is off-screen
  if (gameState.dino.obstacleX > 8) {
    dinoNewObstacle();
  }

  // Collision detection
  playerY = gameState.dino.isCrouching ? 7 : static_cast<int8_t>(gameState.dino.playerY8 / 8);
  bool x_collision = (gameState.dino.obstacleX <= DINO_X_POS) && (DINO_X_POS < gameState.dino.obstacleX + gameState.dino.obstacleW);
  if (x_collision) {
    int8_t obstacleTopY, obstacleBottomY;
    if (gameState.dino.obstacleType == 0) { // Ground
      obstacleTopY = 8 - gameState.dino.obstacleH;
      obstacleBottomY = 7;
    } else { // Flying
      obstacleTopY = gameState.dino.obstacleY;
      obstacleBottomY = gameState.dino.obstacleY + gameState.dino.obstacleH - 1;
    }

    if (playerY >= obstacleTopY && playerY <= obstacleBottomY) {
      beepHit();
      finishGame(false, "Ouch!");
      return;
    }
  }
}

void dinoDraw() {
  clearFrame();

  // Draw scrolling ground line
  // The expression `(i - (millis()/100))` makes it scroll right-to-left
  for (uint8_t i = 0; i < 8; i++) {
    uint32_t groundColor = (( (i - (millis()/100)) % 4) < 2) ? Color(20,20,20) : Color(10,10,10);
    setPixel(i, 7, groundColor);
  }

  // Draw player (dino)
  const int8_t DINO_X_POS = 6;
  int8_t playerY = static_cast<int8_t>(gameState.dino.playerY8 / 8);
  if (gameState.dino.isCrouching) {
    setPixel(DINO_X_POS, 7, Color(0, 40, 10)); // Dark green crouching dino
  } else {
    setPixel(DINO_X_POS, playerY, Color(0, 80, 20)); // Green dino
  }

  // Draw obstacle (cactus)
  for (uint8_t w = 0; w < gameState.dino.obstacleW; ++w) {
    for (uint8_t h = 0; h < gameState.dino.obstacleH; ++h) {
      int8_t px = gameState.dino.obstacleX + w;
      if (px >=0 && px < 8) {
        int8_t py = (gameState.dino.obstacleType == 0) ? (7 - h) : (gameState.dino.obstacleY + h);
        setPixel(px, py, Color(80, 40, 0));
      }
    }
  }
}

// ========================================
// Game Router Functions
// ========================================

void initGame(GameId id) {
  switch (id) {
    case GAME_SNAKE_WALL: snakeInit(false); break;
    case GAME_SNAKE_WRAP: snakeInit(true); break;
    case GAME_TETRIS: tetrisInit(); break;
    case GAME_FLAPPY_EASY: flappyEasyInit(); break;
    case GAME_FLAPPY_HARD: flappyHardInit(); break;
    case GAME_ASTEROIDS_HARD: asteroidsHardInit(); break;
    case GAME_PACMAN_EASY: pacEasyInit(); break;
    case GAME_PACMAN_HARD: pacmanInit(); break;
    case GAME_SPACE_SHOOTER: spaceShooterInit(); break;
    case GAME_BREAKOUT: breakoutInit(); break;
    case GAME_TTT_AI: tttInit(true); break;
    case GAME_TTT_2P: tttInit(false); break;
    case GAME_PONG: pongInit(); break;
    case GAME_TUG: tugInit(); break;
    case GAME_CHECKERS_AI: checkersInit(true); break;
    case GAME_CHECKERS_2P: checkersInit(false); break;
    case GAME_MINESWEEPER: minesweeperInit(); break;
    case GAME_DINO: dinoInit(); break;
    default: break;
  }
}

void updateGame(GameId id, uint32_t now) {
  switch (id) {
    case GAME_SNAKE_WALL:
    case GAME_SNAKE_WRAP: snakeUpdate(now); break;
    case GAME_TETRIS: tetrisUpdate(now); break;
    case GAME_FLAPPY_EASY: flappyEasyUpdate(now); break;
    case GAME_FLAPPY_HARD: flappyHardUpdate(now); break;
    case GAME_ASTEROIDS_HARD: asteroidsHardUpdate(now); break;
    case GAME_PACMAN_EASY: pacEasyUpdate(now); break;
    case GAME_PACMAN_HARD: pacmanUpdate(now); break;
    case GAME_SPACE_SHOOTER: spaceShooterUpdate(now); break;
    case GAME_BREAKOUT: breakoutUpdate(now); break;
    case GAME_TTT_AI:
    case GAME_TTT_2P: tttUpdate(now); break;
    case GAME_PONG: pongUpdate(now); break;
    case GAME_TUG: tugUpdate(now); break;
    case GAME_CHECKERS_AI:
    case GAME_CHECKERS_2P: checkersUpdate(now); break;
    case GAME_MINESWEEPER: minesweeperUpdate(now); break;
    case GAME_DINO: dinoUpdate(now); break;
    default: break;
  }
}

void drawGame(GameId id) {
  switch (id) {
    case GAME_SNAKE_WALL:
    case GAME_SNAKE_WRAP: snakeDraw(); break;
    case GAME_TETRIS: tetrisDraw(); break;
    case GAME_FLAPPY_EASY: flappyEasyDraw(); break;
    case GAME_FLAPPY_HARD: flappyHardDraw(); break;
    case GAME_ASTEROIDS_HARD: asteroidsHardDraw(); break;
    case GAME_PACMAN_EASY: pacEasyDraw(); break;
    case GAME_PACMAN_HARD: pacmanDraw(); break;
    case GAME_SPACE_SHOOTER: spaceShooterDraw(); break;
    case GAME_BREAKOUT: breakoutDraw(); break;
    case GAME_TTT_AI:
    case GAME_TTT_2P: tttDraw(); break;
    case GAME_PONG: pongDraw(); break;
    case GAME_TUG: tugDraw(); break;
    case GAME_CHECKERS_AI:
    case GAME_CHECKERS_2P: checkersDraw(); break;
    case GAME_MINESWEEPER: minesweeperDraw(); break;
    case GAME_DINO: dinoDraw(); break;
    default: clearFrame(); break;
  }
}

// ========================================
// Display Functions
// ========================================

void drawMenuMatrix(uint32_t now) {
  clearFrame();
  uint8_t sweep = static_cast<uint8_t>((now / 140) % MATRIX_W);
  
  for (uint8_t y = 0; y < MATRIX_H; ++y) {
    for (uint8_t x = 0; x < MATRIX_W; ++x) {
      uint8_t r = static_cast<uint8_t>((x * 20 + now / 9) & 0x3F);
      uint8_t g = static_cast<uint8_t>((y * 18 + now / 13) & 0x3F);
      uint8_t b = static_cast<uint8_t>((x * 9 + y * 11 + now / 17) & 0x3F);
      
      if (x == sweep) r = 70;
      
      setPixel(x, y, Color(r / 2, g / 2, b / 2));
    }
  }
}

void drawGameOverMatrix(uint32_t now) {
  clearFrame();
  bool pattern = ((now / 240) % 2) == 0;
  
  for (uint8_t y = 0; y < MATRIX_H; ++y) {
    for (uint8_t x = 0; x < MATRIX_W; ++x) {
      if (((x + y) % 2 == 0) != pattern) continue;
      setPixel(x, y, lastWin ? Color(0, 45, 0) : Color(55, 0, 0));
    }
  }
}

void drawMenuOLED() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  
  oled.setCursor(0, 0);
  oled.print("NB6_Boy");
  oled.setCursor(92, 0);
  oled.print(soundMuted ? "MUTE" : "SND");
  
  int8_t start = menuIndex - 2;
  if (start < 0) start = 0;
  
  int8_t maxStart = static_cast<int8_t>(GAME_COUNT - 5);
  if (maxStart < 0) maxStart = 0;
  if (start > maxStart) start = maxStart;
  
  for (uint8_t i = 0; i < 5; ++i) {
    int8_t gameIdx = static_cast<int8_t>(start + i);
    if (gameIdx >= GAME_COUNT) break;
    
    oled.setCursor(0, 14 + i * 10);
    oled.print(gameIdx == menuIndex ? "> " : "  ");
    oled.print(GAME_NAMES[gameIdx]);
  }
  
  oled.display();
}

void drawPlayingOLED() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  
  oled.setCursor(0, 0);
  oled.print(GAME_NAMES[currentGame]);
  if (soundMuted) {
    oled.setCursor(98, 0);
    oled.print("MUTE");
  }
  
  if (gamePaused) {
    oled.setCursor(0, 16);
    oled.print("*** PAUSED ***");
    oled.setCursor(0, 28);
    oled.print("PAUSE to resume");
  } else {
    oled.setCursor(0, 16);
    oled.print("Score: ");
    oled.print(score);
    
    oled.setCursor(0, 28);
    oled.print("Best: ");
    oled.print(highScore[currentGame]);
  }
  
  oled.setCursor(0, 46);
  oled.print("SEL -> Menu");
  
  oled.display();
}

void drawGameOverOLED() {
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

// ========================================
// Setup and Main Loop
// ========================================

void setup() {
  // Initialize buzzer pin and ensure it's off
  pinMode(BUZZER, OUTPUT);
  noTone(BUZZER);
  
  // Configure I2C for OLED display
  // Wire.setSDA(D4);
  // Wire.setSCL(D5);
  // Pass the pins directly to begin(SDA, SCL)
  Wire.begin(D4, D5);
  Wire.begin();
  
  // Initialize NeoPixel LED matrix
  matrix.begin();
  matrix.clear();
  matrix.show();
  
  // Play startup animation and sound
  playBootSequence();
  
  // Initialize OLED display with error checking
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    // OLED initialization failed - continue anyway
    // Game is still playable with just LED matrix
    // Could add visual error indicator on LED matrix if needed
  }
  
  // Setup button debouncing system
  initButtons();
  
  // Seed random number generator with analog noise + microsecond timer
  // This ensures different random sequences each time the device powers on
  randomSeed(static_cast<uint32_t>(analogRead(A0)) ^ micros());
  
  // Initialize frame timing variables
  lastMatrixMs = millis();
  lastOledMs = millis();
}

void loop() {
  // Get current time once per loop for consistency across all timing checks
  uint32_t now = millis();
  
  // Update button states with debouncing
  // This must be called every loop to properly detect presses/releases
  updateButtons(now);
  
  // ============================================================
  // GLOBAL INPUT HANDLING (works in any state)
  // ============================================================
  
  // SELECT button always returns to menu (except when already in menu)
  if (takePress(B_SELECT) && systemState != STATE_MENU) {
    beepButton();
    returnToMenu();
  }
  
  // ============================================================
  // STATE-SPECIFIC INPUT HANDLING
  // ============================================================
  
  if (systemState == STATE_MENU) {
    bool muteComboDown = isDown(B_LEFT) && isDown(B_RIGHT);
    if (muteComboDown && !menuMuteComboLatched) {
      setSoundMuted(!soundMuted);
      if (!soundMuted) {
        beepButton();
      }
      menuMuteComboLatched = true;
      clearButtonEdges();
    }
    if (!muteComboDown) {
      menuMuteComboLatched = false;
    }
    
    // Menu Navigation
    if (takePress(B_UP)) {
      // Wrap around to bottom when going up from top
      menuIndex = static_cast<int8_t>((menuIndex + GAME_COUNT - 1) % GAME_COUNT);
      beepButton();
    }
    if (takePress(B_DOWN)) {
      // Wrap around to top when going down from bottom
      menuIndex = static_cast<int8_t>((menuIndex + 1) % GAME_COUNT);
      beepButton();
    }
    if (takePress(B_PAUSE)) {
      // PAUSE button starts the selected game
      beepButton();
      startGame(static_cast<GameId>(menuIndex));
    }
    
  } else if (systemState == STATE_PLAYING) {
    // Active Gameplay - delegate to current game's update function
    // Each game handles its own input and logic
    updateGame(currentGame, now);
    
  } else if (systemState == STATE_GAME_OVER) {
    // Game Over Screen - auto-return to menu after 2 seconds OR on any button press
    if ((now - gameOverStartTime >= Timing::GAME_OVER_DELAY_MS) ||
        takePress(B_PAUSE) || 
        takePress(B_SELECT)) {
      beepButton();
      returnToMenu();
    }
  }
  
  // ============================================================
  // SOUND SYSTEM UPDATE
  // ============================================================
  
  // Process sound queue - plays queued tones without blocking
  updateSound(now);
  
  // ============================================================
  // DISPLAY RENDERING (Frame Rate Limited)
  // ============================================================
  
  // Render LED Matrix at ~30 FPS (33ms per frame)
  // This rate provides smooth animations without overwhelming the LEDs
  if (now - lastMatrixMs >= Timing::MATRIX_FRAME_MS) {
    lastMatrixMs = now;
    
    // Draw appropriate visual based on current state
    if (systemState == STATE_MENU) {
      drawMenuMatrix(now);  // Animated color pattern
    } else if (systemState == STATE_PLAYING) {
      drawGame(currentGame);  // Active game graphics
      drawFlashingPauseBorder(now);
    } else {
      drawGameOverMatrix(now);  // Win/lose pattern
    }
    
    // Push frame buffer to physical LEDs
    showFrame();
  }
  
  // Render OLED Screen at ~10 FPS (100ms per frame)
  // Slower rate reduces flicker on OLED displays
  if (now - lastOledMs >= Timing::OLED_FRAME_MS) {
    lastOledMs = now;
    
    // Draw appropriate text/UI based on current state
    if (systemState == STATE_MENU) {
      drawMenuOLED();  // Game list with selection indicator
    } else if (systemState == STATE_PLAYING) {
      drawPlayingOLED();  // Score, high score, pause status
    } else {
      drawGameOverOLED();  // Final score and results
    }
  }
}
