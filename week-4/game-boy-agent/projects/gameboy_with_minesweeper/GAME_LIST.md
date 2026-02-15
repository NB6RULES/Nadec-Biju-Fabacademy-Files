# GameBoy RP2040 - Complete Game List

## Total Games: 17

---

## 1. **Snake (Wall Mode)**
- **Controls:** D-pad to change direction
- **Objective:** Eat food to grow, don't hit walls or yourself
- **Difficulty:** Medium
- **Features:** Dies when hitting walls, classic snake gameplay

---

## 2. **Snake (Wrap Mode)**
- **Controls:** D-pad to change direction
- **Objective:** Eat food to grow, don't hit yourself
- **Difficulty:** Easy
- **Features:** Wraps around edges, no wall collisions

---

## 3. **Tetris**
- **Controls:** 
  - LEFT/RIGHT = Move piece
  - UP = Rotate piece
  - DOWN = Soft drop
- **Objective:** Clear lines by filling rows
- **Difficulty:** Hard
- **Features:** Classic Tetris with 7 piece types, line clearing, increasing speed

---

## 4. **Flappy Bird (Easy)**
- **Controls:** Any button = Flap
- **Objective:** Navigate through pipes
- **Difficulty:** Easy
- **Features:** Slower movement, larger gaps, simplified physics

---

## 5. **Flappy Bird (Hard)**
- **Controls:** Any button = Flap
- **Objective:** Navigate through pipes
- **Difficulty:** Hard
- **Features:** Faster movement, sub-pixel physics, smaller gaps

---

## 6. **Asteroids (Hard)**
- **Controls:**
  - LEFT/RIGHT = Move ship
  - PAUSE = Shoot
- **Objective:** Destroy asteroids, avoid collisions
- **Difficulty:** Hard
- **Features:** Fast-paced, multiple asteroids, bullet management

---

## 7. **Pac-Man (Easy)**
- **Controls:** D-pad to move
- **Objective:** Collect all pellets, avoid ghost
- **Difficulty:** Easy
- **Features:** Simple maze, one ghost, slower movement
- **Pause:** PAUSE button ✓

---

## 8. **Pac-Man (Hard)**
- **Controls:** D-pad to change direction
- **Objective:** Collect all pellets, avoid ghost
- **Difficulty:** Hard
- **Features:** Complex maze, faster gameplay, momentum-based movement
- **Pause:** PAUSE button ✓

---

## 9. **Space Shooter**
- **Controls:**
  - LEFT/RIGHT = Move ship
  - UP = Shoot
- **Objective:** Destroy enemies, avoid enemy fire
- **Difficulty:** Medium-Hard
- **Features:** Enemy ships, bidirectional shooting, increasing difficulty

---

## 10. **Breakout**
- **Controls:** LEFT/RIGHT = Move paddle
- **Objective:** Break all bricks with the ball
- **Difficulty:** Medium
- **Features:** 3 rows of bricks, ball physics, paddle deflection
- **Pause:** PAUSE button ✓

---

## 11. **Tic-Tac-Toe (AI)**
- **Controls:**
  - D-pad = Move cursor
  - PAUSE = Place mark
- **Objective:** Get 3 in a row before AI
- **Difficulty:** Hard (AI uses minimax)
- **Features:** Smart AI opponent, perfect play detection

---

## 12. **Tic-Tac-Toe (2-Player)**
- **Controls:**
  - D-pad = Move cursor
  - PAUSE = Place mark
- **Objective:** Get 3 in a row
- **Difficulty:** Depends on opponent
- **Features:** Local multiplayer, turn-based

---

## 13. **Air Hockey (Pong)**
- **Controls:** LEFT/RIGHT = Move paddle
- **Objective:** Score on AI while defending your goal
- **Difficulty:** Medium
- **Features:** 
  - Horizontal paddles (player at bottom, AI at top)
  - AI opponent with tracking
  - Ball physics with spin
  - Increasing speed
- **Pause:** PAUSE button ✓

---

## 14. **Tug of War (2-Player)**
- **Controls:**
  - Player 1: UP or LEFT
  - Player 2: DOWN, RIGHT, or PAUSE
- **Objective:** Push the indicator to opponent's side
- **Difficulty:** Depends on opponent
- **Features:** Button-mashing competition, local multiplayer

---

## 15. **Checkers (AI)**
- **Controls:**
  - D-pad = Move cursor
  - PAUSE = Select/move piece
- **Objective:** Capture all AI pieces
- **Difficulty:** Hard (AI uses minimax with alpha-beta pruning)
- **Features:** Full checkers rules, kings, forced captures, smart AI

---

## 16. **Checkers (2-Player)**
- **Controls:**
  - D-pad = Move cursor
  - PAUSE = Select/move piece
- **Objective:** Capture all opponent pieces
- **Difficulty:** Depends on opponent
- **Features:** Full checkers rules, kings, forced captures, local multiplayer

---

## 17. **Minesweeper**
- **Controls:**
  - D-pad = Move cursor
  - PAUSE (short press) = Toggle flag
  - PAUSE (long press 500ms) = Reveal cell
- **Objective:** Reveal all safe cells without hitting mines
- **Difficulty:** Medium
- **Features:**
  - 8×8 grid with 10 mines
  - First click always safe
  - Flood-fill for empty areas
  - Color-coded numbers:
    - Purple = Safe zones (0 mines)
    - Green = 1 mine nearby
    - Yellow = 2 mines nearby
    - Orange = 3 mines nearby
    - Red = 4+ mines nearby
  - Red flags for suspected mines

---

## Game Categories

### **Single Player Action:**
- Snake (Wall & Wrap)
- Tetris
- Flappy Bird (Easy & Hard)
- Asteroids Hard
- Pac-Man (Easy & Hard)
- Space Shooter
- Breakout

### **Single Player Strategy:**
- Tic-Tac-Toe AI
- Checkers AI
- Minesweeper

### **Two Player:**
- Tic-Tac-Toe 2P
- Checkers 2P
- Air Hockey (Pong)
- Tug of War 2P

### **Games with AI Opponents:**
- Tic-Tac-Toe AI (Minimax algorithm)
- Checkers AI (Minimax with alpha-beta pruning)
- Air Hockey (Ball tracking AI)
- Pac-Man (Ghost AI)

### **Games with Pause Functionality:**
- Pac-Man Easy ✓
- Pac-Man Hard ✓
- Breakout ✓
- Air Hockey ✓

---

## Hardware Specs

- **Display:** 8×8 RGB LED Matrix (NeoPixel)
- **Screen:** 128×64 OLED (for menus and scores)
- **Controller:** 6 buttons (Up, Down, Left, Right, Pause, Select)
- **Sound:** Piezo buzzer
- **Processor:** RP2040 (Raspberry Pi Pico)

---

## Universal Controls

- **SELECT button:** Return to menu from any game
- **PAUSE button:** Toggle pause (on supported games)

---

## Scoring System

- Each game tracks its own high score
- High scores persist during the session
- Score displayed on OLED screen during gameplay
- Game Over screen shows current score and best score

---

## Visual Design

- Color-coded game elements for easy recognition
- Smooth animations (30 FPS on LED matrix)
- Pulsing cursors for selection
- Game-specific color schemes

---

*Total playtime possibilities: Endless!*
