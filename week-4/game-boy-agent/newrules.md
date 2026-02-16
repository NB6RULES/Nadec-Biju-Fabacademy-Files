# Project Guidelines

You are an automated development assistant for a **Seeed Studio XIAO RP2040–based handheld gaming device** (Gameboy-style). The device includes an **8x8 NeoPixel matrix display**, a **0.96" I2C OLED display**, input buttons, and a buzzer. You can fully automate various tasks according to user requirements.

---
Build a **Game Boy–style handheld gaming system** using a **Seeed Studio XIAO RP2040** with the following hardware and features:

## Hardware

- 8x8 WS2812 NeoPixel LED Matrix (primary display)
- 0.96" SSD1306 OLED (I2C) for UI and score
- Directional buttons: UP, DOWN, LEFT, RIGHT
- Action buttons: PAUSE, GAME SELECT
- Buzzer for sound effects

### Pin Configuration

#define MATRIX_PIN D10  
#define BTN_UP D3  
#define BTN_DOWN D9  
#define BTN_LEFT D2  
#define BTN_RIGHT D7  
#define BTN_PAUSE D6  
#define BUZZER D0  
#define GAME_SELECTOR_BTN D1  

OLED uses I2C on:
- SDA → D4  
- SCL → D5  

---

## System Requirements

- Use **Adafruit NeoPixel** for LED matrix
- Use **Adafruit SSD1306 + GFX** for OLED
- Implement **non-blocking game loops** using `millis()`
- Use **INPUT_PULLUP buttons with debouncing**
- Maintain **frame-based rendering system**
- Use **zig-zag LED mapping (serpentine layout)**

--- 
### NeoPixel Matrix (8x8)

- Type: WS2812 / NeoPixel  
- Data Pin: `MATRIX_PIN`  
- Resolution: 8x8 (64 LEDs)  
- Used for primary game rendering  

### OLED Display (0.96" I2C)

- Interface: I2C  
- SDA: Default RP2040 SDA pin  
- SCL: Default RP2040 SCL pin  
- Used for:
  - Game menus  
  - Score display  
  - Status info  

---

## Core Architecture

### State Machine

Implement a game state system:

- MENU
- Multiple game states (each game has its own logic)
- GAME OVER handling
- Ability to return to menu via GAME_SELECTOR_BTN

---

## Features

### Menu System
- OLED displays list of games
- Navigate using UP/DOWN
- Select using PAUSE
- LED matrix shows animated background

---

### Games to Implement

1. Snake (2 modes: wall collision + wrap around)
2. Tetris (simplified for 8x8 grid)
3. Flappy Bird
4. Asteroids (ship + bullets + falling enemies)
5. Pac-Man (pellets + ghost AI)
6. Space Shooter (vertical shooting)
7. Breakout (paddle + ball + bricks)
8. Tic-Tac-Toe (vs AI)
9. Tic-Tac-Toe (2-player)
10. Pong (player vs AI)
11. Tug of War (button mash game)

---

## Game Mechanics Requirements

- Grid-based movement (8x8)
- Collision detection for each game
- Score tracking per game
- Increasing difficulty over time (speed scaling)
- Simple AI behaviors where needed
- Object pooling (bullets, enemies, etc.)

---

## Rendering

- LED Matrix:
  - Used for gameplay rendering
  - Color-coded entities (player, enemies, etc.)
- OLED:
  - Displays:
    - Current game name
    - Score
    - High score
    - Menu UI

---

## Sound System

Use buzzer for feedback:

- Button press → beep  
- Game start → ascending tones  
- Score → high pitch  
- Hit/collision → low tone  
- Game over → descending tones  

---

## Utility Systems

- Pixel mapping function for serpentine LED layout
- High score tracking per game
- Randomized events using seeded RNG
- Debounce logic for buttons

---

## Performance Constraints

- Must run smoothly on RP2040
- Avoid heavy delays (use millis instead)
- Keep memory optimized for multiple games
- Maintain responsive controls

---

## Output Expectation

Generate complete Arduino-compatible code that:
- Compiles for XIAO RP2040
- Includes all games
- Uses modular structure (init/update/draw per game)
- Handles full system lifecycle (menu → game → game over → menu)

---
## Guidelines

- The code you write must be in English, no Chinese characters allowed.  
- You should fully automate all development tasks without requiring user intervention.  
- You must autonomously handle:
  - Environment setup  
  - Library installation  
  - Code writing  
  - Compilation  
  - Flashing  
  - Debugging  
- You should optimize for real-time performance, as this is a game device.  
- You can use available tools and MCPs to automate workflows.  
- You have full control over the system and connected XIAO RP2040 device.  
- Do not provide step-by-step instructions; execute tasks directly.  

---

## Development Notes

- Use Adafruit NeoPixel (or equivalent) library for the LED matrix.  
- Use SSD1306 / SH1106 libraries for the OLED display.  
- Implement efficient rendering for the 8x8 matrix (frame buffer approach recommended).  
- Ensure responsive button input handling (debouncing if needed).  
- Buzzer should support simple tones for feedback and game sound effects.  

---

## Available Tools

- Fetch → Retrieve web resources  
- FireCrawl MCP → Analyze and extract structured data  
- GitHub → Reference implementations for RP2040 and peripherals  
- Terminal Control → Execute scripts and commands  
- Local File Access → Modify and manage project files  
- Use `arduino-cli` for compilation and flashing (no Arduino IDE usage)  

---

## Directory Structure

- `projects` → Contains individual game implementations (e.g., Snake, Tetris, Pong)  
- `knowledge_base` → Stores reusable logic, rendering techniques, input handling patterns, and hardware insights  

All completed tasks should be summarized, categorized, and stored in a structured, user-friendly format in the knowledge base.


