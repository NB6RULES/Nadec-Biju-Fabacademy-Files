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

## Core Architecture

### State Machine

Implement a game state system:

- MENU
- Multiple game states (each game has its own logic)
- GAME OVER handling
- PAUSE state (must freeze game logic without resetting state)
- Ability to return to menu via GAME_SELECTOR_BTN at any time

---

## Features

### Menu System
- OLED displays list of games
- Navigate using UP/DOWN
- Select using PAUSE
- LED matrix shows animated background

- Must display:
  - Game name
  - High score
  - Current selection

---

### Games to Implement

The system must include ALL of the following games:

1. Snake (Wall mode)
2. Snake (Wrap mode)
3. Tetris
4. Flappy Bird Easy
5. Flappy Bird Hard
6. Asteroids Easy
7. Asteroids Hard
8. Pac-Man
9. Space Shooter
10. Breakout
11. TicTacToe AI
12. TicTacToe 2 Player
13. Pong (Air Hockey style)
14. Pong (Classic vs AI)
15. Tug of War (2 Player)
16. Minesweeper
17. Dodge (avoid falling obstacles)
18. Endless Runner

Each game must have:
- Independent logic and update cycle
- Score tracking
- Increasing difficulty over time
- Proper game over condition

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
    - Pause screen

---

## Pause System

- Triggered using PAUSE button
- Must NOT reset game state
- Must freeze all updates and animations
- OLED must show pause menu with:
  - Resume
  - Exit to Menu
- Navigation via UP/DOWN
- Confirm using PAUSE

---

## Sound System

Use buzzer for feedback:

- Button press → short beep  
- Game start → ascending tones  
- Score → high pitch  
- Hit/collision → low tone  
- Pause → double beep  
- Game over → descending tones  

Sound must not block gameplay loop.

---

## Utility Systems

- Pixel mapping function for serpentine LED layout
- High score tracking per game
- Randomized events using seeded RNG
- Debounce logic for buttons

---

## Firebase Integration

- System must support optional Firebase integration for score storage
- Upload high scores after each game over
- Fetch high scores on startup
- Must fail gracefully if no connection is available
- Must not block gameplay or UI

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
- Includes all games listed
- Uses modular structure (init/update/draw per game)
- Handles full system lifecycle (menu → game → pause → game over → menu)

---

## Guidelines

- The code you write must be in English, no Chinese characters allowed  
- You should fully automate all development tasks without requiring user intervention  
- You must autonomously handle:
  - Environment setup  
  - Library installation  
  - Code writing  
  - Compilation  
  - Flashing  
  - Debugging  
- You should optimize for real-time performance  
- You have full control over the system and connected XIAO RP2040 device  

---

## Development Notes

- Use Adafruit NeoPixel (or equivalent)
- Use SSD1306 / SH1106 libraries
- Ensure efficient rendering for 8x8 matrix
- Ensure responsive input handling
- Buzzer must support multiple tone patterns

---

## Directory Structure

- `projects` → Contains individual game implementations  
- `knowledge_base` → Stores reusable logic, rendering techniques, input handling patterns, and hardware insights  

All completed tasks should be summarized and stored clearly.
