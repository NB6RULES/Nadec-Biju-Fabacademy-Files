# GameBoy RP2040 - Detailed Code Explanation

## Table of Contents
1. [System Architecture](#system-architecture)
2. [Button System](#button-system)
3. [Display System](#display-system)
4. [Sound System](#sound-system)
5. [Game State Management](#game-state-management)
6. [Memory Optimization](#memory-optimization)
7. [Individual Game Implementations](#individual-game-implementations)

---

## System Architecture

### Hardware Configuration
```cpp
// Pin Definitions
#define MATRIX_PIN D10      // NeoPixel LED matrix data pin
#define BTN_UP D3           // Up button
#define BTN_DOWN D9         // Down button
#define BTN_LEFT D2         // Left button
#define BTN_RIGHT D7        // Right button
#define BTN_PAUSE D6        // Pause/Action button
#define BUZZER D0           // Piezo buzzer
#define GAME_SELECTOR_BTN D1 // Menu/Select button
```

### System States
The console operates in three main states:
1. **STATE_MENU** - Browsing game selection
2. **STATE_PLAYING** - Active gameplay
3. **STATE_GAME_OVER** - Game ended, showing results (auto-returns to menu after 2 seconds)

---

## Button System

### How Button Debouncing Works

The button system uses a sophisticated debouncing mechanism to prevent false triggers:

```cpp
struct Button {
  uint8_t pin;           // Arduino pin number
  bool stable;           // Current stable state (debounced)
  bool last;             // Previous stable state
  bool press;            // Rising edge detected (button just pressed)
  bool release;          // Falling edge detected (button just released)
  uint32_t changedAt;    // Timestamp of last state change
};
```

**Debouncing Process:**
1. Read raw pin state
2. If state differs from stable state:
   - Check if enough time has passed (30ms)
   - If yes, update stable state
   - Detect edges (press/release events)
3. Games use `takePress()` to consume one-time events
4. Games use `isDown()` for continuous hold detection

**Example Usage:**
```cpp
// One-time press (consumed after use)
if (takePress(B_UP)) {
    // This triggers once per button press
}

// Continuous hold (triggers every frame while held)
if (isDown(B_LEFT)) {
    // This triggers continuously while button is held
}
```

---

## Display System

### LED Matrix (8×8 NeoPixel)

**Frame Buffer Architecture:**
- Double buffering prevents flicker
- Frame buffer holds RGB values for all 64 LEDs
- Updates at ~30 FPS (33ms per frame)

**Coordinate System:**
```
(0,0) ──────→ (7,0)
  │              │
  │    Matrix    │
  │              │
(0,7) ──────→ (7,7)
```

**LED Index Mapping (Serpentine):**
The physical LED strip is laid out in a serpentine pattern:
```
Row 0: →  0  1  2  3  4  5  6  7
Row 1: ← 15 14 13 12 11 10  9  8
Row 2: → 16 17 18 19 20 21 22 23
Row 3: ← 31 30 29 28 27 26 25 24
...
```

The `ledIndex()` function converts (x,y) to physical LED position:
```cpp
uint8_t ledIndex(uint8_t x, uint8_t y) {
  return (y % 2 == 0) ?  // Even rows go left-to-right
    y * 8 + x :          // Odd rows go right-to-left
    y * 8 + (7 - x);
}
```

**Drawing Pipeline:**
1. Game calls `clearFrame()` to reset buffer
2. Game calls `setPixel(x, y, color)` for each element
3. Main loop calls `showFrame()` to push buffer to LEDs
4. NeoPixel library handles the actual LED communication

### OLED Screen (128×64)

- Displays game menus, scores, and status
- Updates at ~10 FPS (100ms per frame) - slower to reduce flicker
- Shows:
  - Menu: Game list with scrolling
  - Playing: Game name, score, high score, controls
  - Game Over: Result, final score, high score

---

## Sound System

### Non-Blocking Tone Queue

Traditional `tone()` function blocks code execution. This system uses a queue:

**Queue Structure:**
```cpp
struct ToneEvent {
  uint16_t freq;      // Frequency in Hz
  uint16_t duration;  // Duration in milliseconds
};

ToneEvent toneQueue[24];  // Circular buffer
```

**How It Works:**
1. Sound effects call `enqueueTone(freq, duration)` to add sounds
2. Queue uses head/tail pointers (circular buffer)
3. Main loop calls `updateSound()` every frame
4. Plays one tone at a time without blocking
5. Multiple sounds can be queued for sequences

**Predefined Sounds:**
- `beepButton()` - 1200 Hz, 26ms - UI feedback
- `beepScore()` - 1600 Hz, 35ms - Scoring event
- `beepHit()` - 240 Hz, 90ms - Damage/collision
- `beepStart()` - 3-tone sequence - Game start
- `beepLose()` - Descending 3-tone - Game over
- `beepWin()` - Ascending 3-tone - Victory

---

## Game State Management

### Memory-Optimized Union

Instead of allocating memory for ALL games simultaneously, a C++ union is used:

```cpp
union GameState {
  Snake snake;
  Tetris tetris;
  Flappy flappyHard;
  // ... all game states ...
  Minesweeper minesweeper;
};

GameState gameState;  // Only ONE game's memory active at a time
```

**Why This Works:**
- Only one game runs at a time
- Union allocates memory for the LARGEST structure
- Saves ~2-3 KB of RAM on RP2040
- Game state is initialized when game starts

**Example:**
```cpp
// When starting Snake
void snakeInit() {
  memset(&gameState.snake, 0, sizeof(Snake));  // Clear memory
  gameState.snake.x[0] = 4;  // Initialize snake position
  // ... setup continues ...
}
```

### Game Router Pattern

Three main router functions handle all games:

```cpp
void initGame(GameId id) {
  switch (id) {
    case GAME_SNAKE_WALL: snakeInit(false); break;
    case GAME_MINESWEEPER: minesweeperInit(); break;
    // ... handles initialization for each game
  }
}

void updateGame(GameId id, uint32_t now) {
  // Updates game logic each frame
}

void drawGame(GameId id) {
  // Renders game to LED matrix
}
```

**Game Loop Flow:**
```
setup() → Initialize hardware
  ↓
loop() → Read buttons
  ↓
  → Update state machine (menu/playing/gameover)
  ↓
  → Call updateGame() if playing
  ↓
  → Render to LED matrix (30 FPS)
  ↓
  → Render to OLED (10 FPS)
  ↓
  → Update sound queue
  ↓
  → Repeat
```

---

## Memory Optimization

### Techniques Used

1. **Union for Game States** (saves ~2 KB)
2. **const arrays for static data** (stored in flash, not RAM)
3. **uint8_t/int8_t instead of int** (saves 3 bytes per variable)
4. **Bit packing where possible**
5. **Reusing variables** (e.g., `now` timestamp passed to functions)

### RAM Budget (RP2040 has 264 KB)

Approximate usage:
- Game state union: ~500 bytes
- Frame buffer: 256 bytes (64 LEDs × 4 bytes)
- Button states: ~60 bytes
- Sound queue: ~100 bytes
- Stack/heap: ~50 KB reserved
- **Total used: ~1 KB + libraries**

Plenty of headroom for expansion!

---

## Individual Game Implementations

### 1. Snake (Wall & Wrap Modes)

**Data Structure:**
```cpp
struct Snake {
  int8_t x[64];       // X coordinates of snake body (max 64 segments)
  int8_t y[64];       // Y coordinates of snake body
  uint8_t length;     // Current snake length
  int8_t dx, dy;      // Current direction
  int8_t nextDx, nextDy;  // Queued direction (prevents instant 180° turns)
  int8_t foodX, foodY;    // Food position
  bool wrapMode;      // true = wrap around edges, false = die at walls
  uint16_t moveInterval;  // Milliseconds between moves
  uint32_t lastMove;  // Timestamp of last movement
};
```

**How Direction Queuing Works:**
Players often press buttons faster than the snake moves. The queue prevents:
```
Current direction: RIGHT
Player presses: DOWN, LEFT (quickly)
Without queue: Snake would turn LEFT (opposite of RIGHT) = death
With queue: DOWN is queued, then LEFT is queued after DOWN executes
```

**Collision Detection:**
```cpp
// Check if head hits body
for (uint8_t i = 1; i < length; ++i) {
  if (x[0] == x[i] && y[0] == y[i]) {
    // Game over!
  }
}
```

**Food Placement Algorithm:**
1. Generate random (x, y)
2. Check if position overlaps snake body
3. If yes, try again (max 100 attempts)
4. If no valid position found, player wins (board full)

---

### 2. Tetris

**Piece Rotation System:**

Each piece type has 4 rotation states stored as 4×4 bit patterns:

```cpp
// Example: I-piece (line piece)
Rotation 0:    Rotation 1:    Rotation 2:    Rotation 3:
. . . .        . . ■ .        . . . .        . ■ . .
■ ■ ■ ■        . . ■ .        ■ ■ ■ ■        . ■ . .
. . . .        . . ■ .        . . . .        . ■ . .
. . . .        . . ■ .        . . . .        . ■ . .
```

**Collision Detection:**
```cpp
bool tetrisCollision(x, y, rotation) {
  // Check all 4×4 cells of piece pattern
  for (uint8_t py = 0; py < 4; ++py) {
    for (uint8_t px = 0; px < 4; ++px) {
      if (piecePattern[type][rotation][py][px]) {
        int8_t boardX = x + px;
        int8_t boardY = y + py;
        
        // Check bounds
        if (boardX < 0 || boardX >= 8 || boardY >= 8) return true;
        
        // Check existing blocks
        if (boardY >= 0 && board[boardY][boardX]) return true;
      }
    }
  }
  return false;
}
```

**Line Clearing Algorithm:**
1. Lock piece into board array
2. Check each row from bottom to top
3. If row is full (all 8 cells occupied):
   - Remove that row
   - Shift all rows above down by 1
   - Add empty row at top
   - Increment score

---

### 3. Minesweeper

**Board Generation (First Click Safe):**

The board is NOT generated until the player's first click:

```cpp
void minesweeperPlaceMines(int8_t safeX, int8_t safeY) {
  uint8_t minesPlaced = 0;
  
  while (minesPlaced < 10) {
    int8_t x = random(8);
    int8_t y = random(8);
    
    // Skip if already a mine OR the first click position
    if (board[y][x] == 9 || (x == safeX && y == safeY)) {
      continue;
    }
    
    board[y][x] = 9;  // Place mine
    minesPlaced++;
  }
  
  // Calculate numbers for all non-mine cells
  calculateAdjacentMines();
}
```

**Flood Fill Algorithm (Revealing Empty Areas):**

When an empty cell (0 adjacent mines) is revealed, automatically reveal neighbors:

```cpp
void minesweeperFloodFill(int8_t x, int8_t y) {
  // Boundary check
  if (x < 0 || x >= 8 || y < 0 || y >= 8) return;
  
  // Already revealed or flagged?
  if (revealed[y][x] != 0) return;
  
  // Reveal this cell
  revealed[y][x] = 1;
  cellsToReveal--;
  score++;
  
  // If empty (0 adjacent mines), reveal all 8 neighbors
  if (board[y][x] == 0) {
    for (int8_t dy = -1; dy <= 1; ++dy) {
      for (int8_t dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) continue;  // Skip self
        minesweeperFloodFill(x + dx, y + dy);  // Recursive call
      }
    }
  }
}
```

**Long Press Detection:**
```cpp
static uint32_t pausePressStart = 0;
static bool pauseWasPressed = false;

if (isDown(B_PAUSE)) {
  if (!pauseWasPressed) {
    pausePressStart = now;  // Start timing
    pauseWasPressed = true;
  } else if (now - pausePressStart >= 500) {  // 500ms = long press
    revealCell();  // Trigger action
    pauseWasPressed = false;  // Prevent repeat
  }
} else if (pauseWasPressed) {
  // Button released - was it a short press?
  if (now - pausePressStart < 500) {
    toggleFlag();  // Short press action
  }
  pauseWasPressed = false;
}
```

---

### 4. Checkers (AI Mode)

**Board Representation:**
```cpp
uint8_t board[8][8];
// Values:
// 0 = empty
// 1 = Player 1 regular piece
// 2 = Player 1 king
// 3 = Player 2 (AI) regular piece
// 4 = Player 2 (AI) king
```

**AI Algorithm: Minimax with Alpha-Beta Pruning**

The AI searches the game tree to find the best move:

```cpp
int16_t minimax(depth, alpha, beta, maximizing) {
  // Base case: max depth or game over
  if (depth == 0 || gameOver) {
    return evaluateBoard();  // Score current position
  }
  
  if (maximizing) {  // AI's turn (maximize score)
    int16_t maxScore = -32000;
    for each possible move:
      simulate move on board
      score = minimax(depth-1, alpha, beta, false)
      undo move
      
      maxScore = max(maxScore, score)
      alpha = max(alpha, score)
      
      if (beta <= alpha) break;  // Alpha-beta cutoff (prune branch)
    
    return maxScore;
  } else {  // Player's turn (minimize score)
    int16_t minScore = 32000;
    for each possible move:
      simulate move on board
      score = minimax(depth-1, alpha, beta, true)
      undo move
      
      minScore = min(minScore, score)
      beta = min(beta, score)
      
      if (beta <= alpha) break;  // Prune
    
    return minScore;
  }
}
```

**Board Evaluation Function:**
```cpp
int16_t evaluateBoard() {
  int16_t score = 0;
  
  for each piece on board:
    if (AI piece):
      score += 100;  // Regular piece value
      if (king): score += 50;  // King bonus
      if (on back row): score += 30;  // Defensive position
      if (center): score += 10;  // Center control
    
    if (player piece):
      score -= 100;  // Subtract opponent value
      // ... same bonuses but negative
  
  return score;  // Positive = good for AI, negative = good for player
}
```

**Forced Capture Rule:**
If any piece can capture, the player MUST choose a capturing move:

```cpp
void checkersFindValidMoves() {
  bool canCapture = false;
  
  // First pass: check if any captures are possible
  for each piece:
    if (canCaptureFrom(x, y)):
      canCapture = true
      break
  
  // Second pass: find valid moves
  if (canCapture):
    // Only add capturing moves to validMoves[]
  else:
    // Add all legal non-capturing moves
}
```

---

### 5. Air Hockey (Pong Replacement)

**Sub-Pixel Movement:**

To achieve smooth ball movement on an 8×8 grid, positions are stored at 8× resolution:

```cpp
int16_t ballX8;  // Ball X position × 8
int16_t ballY8;  // Ball Y position × 8

// Update (smooth movement)
ballX8 += velocityX;  // Move by 1-3 units per frame
ballY8 += velocityY;

// Render (convert to pixel position)
int8_t ballX = ballX8 / 8;  // Integer division gives pixel coordinate
int8_t ballY = ballY8 / 8;
setPixel(ballX, ballY, color);
```

**Why 8×?**
- At 80ms update interval, ball moves 1-2 pixels per frame
- Without sub-pixel, movement is jerky
- 8× allows velocities like 0.125, 0.25, 0.5 pixels/frame
- Creates smooth diagonal trajectories

**Paddle Hit Detection with Spin:**
```cpp
if (ballY >= 7 && velocityY > 0) {  // Ball approaching player paddle
  if (ballX >= playerX && ballX < playerX + paddleSize) {
    // Paddle hit!
    velocityY = -velocityY;  // Bounce back
    
    // Add spin based on hit position
    int8_t hitPos = ballX - playerX;
    if (hitPos == 0) {
      velocityX = -1;  // Hit left edge = left spin
    } else {
      velocityX = 1;   // Hit right edge = right spin
    }
  }
}
```

**AI Tracking:**
```cpp
void updateAI() {
  // AI tries to center paddle on ball
  int8_t targetX = ballX - (paddleSize / 2);
  
  if (aiX < targetX) {
    aiX++;  // Move right
  } else if (aiX > targetX) {
    aiX--;  // Move left
  }
}
```

AI is deliberately slowed (120ms delay vs player's 60ms) to make it beatable.

---

## Advanced Techniques

### Pause Implementation Pattern

All pausable games follow this pattern:

```cpp
void gameUpdate(uint32_t now) {
  // Check pause button FIRST
  if (takePress(B_PAUSE)) {
    gamePaused = !gamePaused;  // Toggle pause state
    beepButton();
    return;  // Exit early, don't process game logic
  }
  
  // If paused, don't update game
  if (gamePaused) return;
  
  // Normal game logic continues here...
  movePlayer();
  checkCollisions();
  updateEnemies();
}
```

The OLED screen detects pause state and displays pause message.

---

### Difficulty Scaling

Many games increase difficulty over time:

**Snake:**
```cpp
moveInterval -= 5;  // Move faster each food eaten
if (moveInterval < 100) moveInterval = 100;  // Cap at 100ms
```

**Tetris:**
```cpp
dropInterval -= score * 2;  // Faster drops at higher scores
if (dropInterval < 200) dropInterval = 200;  // Minimum 200ms
```

**Air Hockey:**
```cpp
if (score > 0 && score % 10 == 0) {  // Every 10 points
  moveInterval -= 3;  // Speed up
  if (moveInterval < 50) moveInterval = 50;  // Cap
}
```

---

### Frame Rate Management

Different update rates optimize performance:

```cpp
void loop() {
  uint32_t now = millis();
  
  // LED Matrix: 30 FPS (33ms) - smooth animations
  if (now - lastMatrixMs >= 33) {
    lastMatrixMs = now;
    drawGame();
    showFrame();
  }
  
  // OLED Screen: 10 FPS (100ms) - reduces flicker
  if (now - lastOledMs >= 100) {
    lastOledMs = now;
    drawOLED();
  }
  
  // Game logic: As fast as possible
  updateGame(now);
}
```

---

## Debugging Tips

### Common Issues and Solutions

**Issue: Buttons not responding**
- Check debounce timing (30ms)
- Verify `updateButtons()` is called each frame
- Use `takePress()` for one-time events, `isDown()` for continuous

**Issue: Flickering display**
- Ensure frame buffer is cleared each frame
- Don't call `matrix.show()` multiple times per frame
- Use frame rate limiting (33ms minimum)

**Issue: Sound cutting out**
- Tone queue may be full (24 max)
- Check `updateSound()` is called regularly
- Reduce number of simultaneous sound effects

**Issue: Game running too slow/fast**
- Verify timing uses `millis()` not `delay()`
- Check moveInterval values are reasonable
- Profile with Serial.print() to find bottlenecks

---

## Extension Ideas

### Adding a New Game

1. **Define structure in union:**
```cpp
struct NewGame {
  int8_t playerX;
  uint32_t lastUpdate;
  // ... game-specific variables
};

union GameState {
  // ... existing games
  NewGame newgame;
};
```

2. **Add to enum:**
```cpp
enum GameId : uint8_t {
  // ... existing games
  GAME_NEWGAME,
  GAME_COUNT
};

const char *GAME_NAMES[] = {
  // ... existing names
  "My New Game"
};
```

3. **Implement three functions:**
```cpp
void newgameInit() {
  memset(&gameState.newgame, 0, sizeof(NewGame));
  // Initialize game state
}

void newgameUpdate(uint32_t now) {
  // Handle input
  // Update game logic
}

void newgameDraw() {
  clearFrame();
  // Draw game elements
}
```

4. **Add to router:**
```cpp
void initGame(GameId id) {
  switch (id) {
    // ... existing cases
    case GAME_NEWGAME: newgameInit(); break;
  }
}
// Same for updateGame() and drawGame()
```

Done! Your game is now playable!

---

## Performance Considerations

### CPU Usage
- Main loop runs at ~1000 Hz when no delays
- Button debouncing: <1% CPU
- LED rendering: ~5% CPU (limited by NeoPixel library)
- OLED rendering: ~10% CPU
- Game logic: 5-30% depending on game
- **Total: ~30-50% CPU usage** - plenty of headroom!

### Memory Usage
- Stack: ~10 KB
- Heap: ~5 KB
- Static data: ~1 KB
- Libraries: ~40 KB
- **Total: ~56 KB of 264 KB used** - 21% utilization

---

This architecture provides a solid foundation for a multi-game handheld console with room to grow!
