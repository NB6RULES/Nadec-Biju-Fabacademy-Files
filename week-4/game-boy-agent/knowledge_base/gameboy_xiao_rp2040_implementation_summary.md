# GameBoy RP2040 Implementation Summary

## Scope
- Target: Seeed Studio XIAO RP2040 handheld
- Primary display: 8x8 WS2812 (NeoPixel) matrix
- Secondary display: 0.96" SSD1306 OLED (I2C)
- Inputs: UP, DOWN, LEFT, RIGHT, PAUSE, GAME_SELECTOR
- Audio: buzzer feedback and game event tones

## Hardware Mapping
- Matrix data pin: `D10`
- Buttons: `UP D3`, `DOWN D9`, `LEFT D2`, `RIGHT D7`, `PAUSE D6`, `GAME_SELECTOR D1`
- Buzzer pin: `D0`
- OLED I2C: `SDA D4`, `SCL D5`

## Core Runtime Architecture
- Main finite states:
  - `MENU`
  - `PLAYING`
  - `GAME_OVER`
- Non-blocking timing:
  - Uses `millis()` for update cadence and render cadence
  - No gameplay `delay()` loops
- Debounced input:
  - `INPUT_PULLUP` buttons
  - edge detection (`press`/`release`) with debounce window
- Frame-based rendering:
  - 8x8 frame buffer
  - serpentine (zig-zag) LED mapping
  - per-frame flush to NeoPixel hardware
- OLED usage:
  - menu list
  - active game + score + high score
  - game over state summary

## Audio System
- Tone queue for non-blocking buzzer playback
- Events covered:
  - button press
  - game start
  - score events
  - hit/collision
  - game over
  - win event

## Implemented Games
- Snake (wall collision mode)
- Snake (wrap-around mode)
- Tetris (8x8 simplified)
- Flaay Bird Easy (legacy physics from prior user sketch)
- Flappy Bird Hard
- Asteroids Hard (object-pooled bullets + falling rocks)
- Pac-Man Easy (open-grid pellet mode from prior user sketch)
- Pac-Man Hard (4 worlds, pellets + wall mazes + ghost chase AI)
- Space Shooter (advanced mode, slowed pacing)
- Breakout
- Tic-Tac-Toe vs AI
- Tic-Tac-Toe 2-player
- Pong (horizontal paddles, no center line)
- Tug of War 2-player (no AI)

## Boot Matrix Sequence
- On boot, matrix shows:
  - `N` (color frame)
  - `B` (color frame)
  - `6` (color frame)
- Frame rate: `0.75 FPS` (about 1333 ms per frame)
- Followed by a pulsing heart pattern for 4 seconds
- NB6 glyph orientation adjusted (not mirrored)
- Heart pulse duration extended to 4 seconds
- Pac-Man Hard colors tuned: light-yellow pellets and orange Pac-Man

## Shared Gameplay Systems
- Per-game score tracking
- Per-game high score tracking (runtime memory)
- Difficulty scaling by score/time in each game loop
- Object pooling for projectile/enemy-heavy games

## File Inventory
- Main sketch: `projects/gameboy_xiao_rp2040/gameboy_xiao_rp2040.ino`
- Summary record: `knowledge_base/gameboy_xiao_rp2040_implementation_summary.md`
