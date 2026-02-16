import machine
import neopixel
import time
import random
import math
import framebuf

# Try to import ssd1306, handle if missing (user needs to install it)
try:
    import ssd1306
except ImportError:
    print("Error: ssd1306 library not found. Please install it via mip or upload ssd1306.py")
    ssd1306 = None

# ========================================
# Hardware Constants (XIAO RP2040)
# ========================================
# Mapping D-pins to GPIO
# D0=26, D1=27, D2=28, D3=29, D4=6, D5=7, D6=0, D7=1, D8=2, D9=4, D10=3

PIN_MATRIX = 3      # D10
PIN_BTN_UP = 29     # D3
PIN_BTN_DOWN = 4    # D9
PIN_BTN_LEFT = 28   # D2
PIN_BTN_RIGHT = 1   # D7
PIN_BTN_PAUSE = 0   # D6
PIN_BTN_SELECT = 27 # D1
PIN_BUZZER = 26     # D0
PIN_SDA = 6         # D4
PIN_SCL = 7         # D5

MATRIX_W = 8
MATRIX_H = 8
MATRIX_COUNT = 64
SCREEN_W = 128
SCREEN_H = 64

# ========================================
# Timing Constants
# ========================================
BTN_DEBOUNCE_MS = 30
MATRIX_FRAME_MS = 33
OLED_FRAME_MS = 100
GAME_OVER_DELAY_MS = 2000

# ========================================
# Game IDs
# ========================================
GAME_SNAKE_WALL = 0
GAME_SNAKE_WRAP = 1
GAME_TETRIS = 2
GAME_FLAPPY_EASY = 3
GAME_FLAPPY_HARD = 4
GAME_ASTEROIDS_HARD = 5
GAME_PACMAN_EASY = 6
GAME_PACMAN_HARD = 7
GAME_SPACE_SHOOTER = 8
GAME_BREAKOUT = 9
GAME_TTT_AI = 10
GAME_TTT_2P = 11
GAME_PONG = 12
GAME_TUG = 13
GAME_CHECKERS_AI = 14
GAME_CHECKERS_2P = 15
GAME_MINESWEEPER = 16
GAME_DINO = 17
GAME_COUNT = 18

GAME_NAMES = [
    "Snake (Wall)", "Snake (Wrap)", "Tetris", "Flappy Easy", "Flappy Hard",
    "Asteroids Hard", "Pac-Man Easy", "Pac-Man Hard", "Space Shooter", "Breakout",
    "TicTacToe AI", "TicTacToe 2P", "Pong", "Tug of War", "Checkers AI",
    "Checkers 2P", "Minesweeper", "Dino Run"
]

# ========================================
# System State
# ========================================
STATE_MENU = 0
STATE_PLAYING = 1
STATE_GAME_OVER = 2

system_state = STATE_MENU
current_game_id = 0
menu_index = 0
score = 0
high_score = [0] * GAME_COUNT
last_win = False
last_msg = "Game Over"
game_paused = False
sound_muted = False
game_over_start_time = 0

# ========================================
# Hardware Objects
# ========================================
np = neopixel.NeoPixel(machine.Pin(PIN_MATRIX), MATRIX_COUNT)

i2c = machine.I2C(1, sda=machine.Pin(PIN_SDA), scl=machine.Pin(PIN_SCL), freq=400000)
oled = None
if ssd1306:
    try:
        oled = ssd1306.SSD1306_I2C(SCREEN_W, SCREEN_H, i2c)
    except:
        pass

buzzer = machine.PWM(machine.Pin(PIN_BUZZER))
buzzer.duty_u16(0) # Off

# Buttons
buttons = []
class Button:
    def __init__(self, pin_id):
        self.pin = machine.Pin(pin_id, machine.Pin.IN, machine.Pin.PULL_UP)
        self.stable = self.pin.value() == 0
        self.last = self.stable
        self.press = False
        self.release = False
        self.changed_at = 0

btn_pins = [PIN_BTN_UP, PIN_BTN_DOWN, PIN_BTN_LEFT, PIN_BTN_RIGHT, PIN_BTN_PAUSE, PIN_BTN_SELECT]
for p in btn_pins:
    buttons.append(Button(p))

B_UP, B_DOWN, B_LEFT, B_RIGHT, B_PAUSE, B_SELECT = range(6)

# ========================================
# Sound System
# ========================================
tone_queue = []
tone_active = False
tone_end_time = 0

def tone(freq, duration):
    if sound_muted: return
    buzzer.freq(int(freq))
    buzzer.duty_u16(32768) # 50% duty

def no_tone():
    buzzer.duty_u16(0)

def enqueue_tone(freq, duration):
    if sound_muted: return
    if len(tone_queue) < 24:
        tone_queue.append((freq, duration))

def update_sound(now):
    global tone_active, tone_end_time
    if sound_muted:
        if tone_active:
            no_tone()
            tone_active = False
        tone_queue.clear()
        return

    if tone_active and now >= tone_end_time:
        no_tone()
        tone_active = False

    if not tone_active and tone_queue:
        freq, duration = tone_queue.pop(0)
        tone(freq, duration)
        tone_end_time = now + duration
        tone_active = True

def beep_button(): enqueue_tone(1200, 26)
def beep_score(): enqueue_tone(1600, 35)
def beep_hit(): enqueue_tone(240, 90)
def beep_win():
    enqueue_tone(800, 50)
    enqueue_tone(1050, 50)
    enqueue_tone(1400, 80)
def beep_lose():
    enqueue_tone(1000, 60)
    enqueue_tone(700, 70)
    enqueue_tone(430, 90)
def beep_start():
    enqueue_tone(500, 45)
    enqueue_tone(850, 45)
    enqueue_tone(1200, 65)

# ========================================
# Display Utils
# ========================================
frame_buffer = [(0,0,0)] * MATRIX_COUNT

def led_index(x, y):
    if y % 2 == 0:
        return y * MATRIX_W + x
    else:
        return y * MATRIX_W + (MATRIX_W - 1 - x)

def clear_frame(color=(0,0,0)):
    for i in range(MATRIX_COUNT):
        frame_buffer[i] = color

def set_pixel(x, y, color):
    if 0 <= x < MATRIX_W and 0 <= y < MATRIX_H:
        frame_buffer[led_index(x, y)] = color

def show_frame():
    for i in range(MATRIX_COUNT):
        np[i] = frame_buffer[i]
    np.write()

# ========================================
# Input Handling
# ========================================
def update_buttons(now):
    for b in buttons:
        raw = (b.pin.value() == 0)
        if raw != b.last:
            b.last = raw
            b.changed_at = now
        
        if now - b.changed_at >= BTN_DEBOUNCE_MS and raw != b.stable:
            b.stable = raw
            if raw:
                b.press = True
            else:
                b.release = True

def take_press(btn_id):
    if buttons[btn_id].press:
        buttons[btn_id].press = False
        return True
    return False

def is_down(btn_id):
    return buttons[btn_id].stable

def clear_button_edges():
    for b in buttons:
        b.press = False
        b.release = False

# ========================================
# Game Logic Base
# ========================================
class Game:
    def init(self): pass
    def update(self, now): pass
    def draw(self): pass

current_game_obj = None

def finish_game(win, msg):
    global system_state, last_win, last_msg, game_over_start_time, score
    if system_state != STATE_PLAYING: return
    
    if score > high_score[current_game_id]:
        high_score[current_game_id] = score
    
    last_win = win
    last_msg = msg if msg else ("Win" if win else "Game Over")
    system_state = STATE_GAME_OVER
    game_over_start_time = time.ticks_ms()
    clear_button_edges()
    
    if win: beep_win()
    else: beep_lose()

# ========================================
# Games
# ========================================

# --- Snake ---
class SnakeGame(Game):
    def __init__(self, wrap):
        self.wrap = wrap
        self.x = [0]*64
        self.y = [0]*64
        self.length = 0
        self.dx = 0
        self.dy = 0
        self.next_dx = 0
        self.next_dy = 0
        self.food_x = 0
        self.food_y = 0
        self.move_interval = 0
        self.last_move = 0

    def place_food(self):
        for _ in range(100):
            fx = random.randint(0, 7)
            fy = random.randint(0, 7)
            collision = False
            for i in range(self.length):
                if self.x[i] == fx and self.y[i] == fy:
                    collision = True
                    break
            if not collision:
                self.food_x = fx
                self.food_y = fy
                return
        self.food_x = 0
        self.food_y = 0

    def init(self):
        self.length = 3
        self.x[0], self.y[0] = 4, 4
        self.x[1], self.y[1] = 3, 4
        self.x[2], self.y[2] = 2, 4
        self.dx, self.dy = 1, 0
        self.next_dx, self.next_dy = 1, 0
        self.move_interval = 280
        self.last_move = time.ticks_ms()
        self.place_food()

    def update(self, now):
        global score, game_paused
        if take_press(B_PAUSE):
            game_paused = not game_paused
            beep_button()
        if game_paused: return

        if take_press(B_UP) and self.dy != 1: self.next_dx, self.next_dy = 0, -1; beep_button()
        elif take_press(B_DOWN) and self.dy != -1: self.next_dx, self.next_dy = 0, 1; beep_button()
        elif take_press(B_LEFT) and self.dx != 1: self.next_dx, self.next_dy = -1, 0; beep_button()
        elif take_press(B_RIGHT) and self.dx != -1: self.next_dx, self.next_dy = 1, 0; beep_button()

        if time.ticks_diff(now, self.last_move) < self.move_interval: return
        self.last_move = now

        self.dx, self.dy = self.next_dx, self.next_dy
        nx = self.x[0] + self.dx
        ny = self.y[0] + self.dy

        if self.wrap:
            nx %= 8
            ny %= 8
        elif nx < 0 or nx > 7 or ny < 0 or ny > 7:
            beep_hit()
            finish_game(False, "Hit wall")
            return

        for i in range(self.length):
            if self.x[i] == nx and self.y[i] == ny:
                beep_hit()
                finish_game(False, "Hit body")
                return

        ate = (nx == self.food_x and ny == self.food_y)
        if ate and self.length < 64:
            self.length += 1
        
        for i in range(self.length - 1, 0, -1):
            self.x[i] = self.x[i-1]
            self.y[i] = self.y[i-1]
        self.x[0] = nx
        self.y[0] = ny

        if ate:
            score += 10
            beep_score()
            if self.length == 64:
                finish_game(True, "Board full")
                return
            self.place_food()
            if self.move_interval > 100:
                self.move_interval -= 10

    def draw(self):
        clear_frame()
        head_c = (150, 20, 150) if self.wrap else (30, 130, 30)
        body_c = (90, 35, 90) if self.wrap else (20, 70, 20)
        food_c = (0, 85, 85) if self.wrap else (85, 12, 12)
        
        set_pixel(self.food_x, self.food_y, food_c)
        for i in range(self.length):
            set_pixel(self.x[i], self.y[i], head_c if i == 0 else body_c)

# --- Tetris ---
TETRIS_SHAPES = [
    [[[0,1,2,3],[1,1,1,1]], [[2,2,2,2],[0,1,2,3]], [[0,1,2,3],[2,2,2,2]], [[1,1,1,1],[0,1,2,3]]], # I
    [[[1,2,1,2],[0,0,1,1]], [[1,2,1,2],[0,0,1,1]], [[1,2,1,2],[0,0,1,1]], [[1,2,1,2],[0,0,1,1]]], # O
    [[[1,0,1,2],[0,1,1,1]], [[1,1,2,1],[0,1,1,2]], [[0,1,2,1],[1,1,1,2]], [[1,0,1,1],[0,1,1,2]]], # T
    [[[0,0,0,1],[0,1,2,2]], [[0,1,2,2],[1,1,1,0]], [[0,1,1,1],[0,0,1,2]], [[0,0,1,2],[2,1,1,1]]], # L
    [[[1,2,0,1],[0,0,1,1]], [[1,1,2,2],[0,1,1,2]], [[1,2,0,1],[1,1,2,2]], [[0,0,1,1],[0,1,1,2]]]  # Z
]

class TetrisGame(Game):
    def __init__(self):
        self.board = [[0]*8 for _ in range(8)]
        self.px = 0
        self.py = 0
        self.ptype = 0
        self.prot = 0
        self.drop_interval = 0
        self.last_drop = 0
        self.soft_drop_time = 0

    def can_place(self, x, y, t, r):
        shape = TETRIS_SHAPES[t][r]
        for i in range(4):
            bx = x + shape[0][i]
            by = y + shape[1][i]
            if bx < 0 or bx > 7 or by < 0 or by > 7: return False
            if self.board[by][bx]: return False
        return True

    def spawn(self):
        self.ptype = random.randint(0, 4)
        self.prot = 0
        self.px = 2
        self.py = 0
        if not self.can_place(self.px, self.py, self.ptype, self.prot):
            beep_hit()
            finish_game(False, "Stacked out")

    def lock(self):
        shape = TETRIS_SHAPES[self.ptype][self.prot]
        for i in range(4):
            bx = self.px + shape[0][i]
            by = self.py + shape[1][i]
            if 0 <= bx < 8 and 0 <= by < 8:
                self.board[by][bx] = self.ptype + 1

    def clear_lines(self):
        global score
        lines = 0
        y = 7
        while y >= 0:
            if all(self.board[y]):
                lines += 1
                for row in range(y, 0, -1):
                    self.board[row] = self.board[row-1][:]
                self.board[0] = [0]*8
                # Don't decrement y, check same row again
            else:
                y -= 1
        if lines:
            score += lines * 10
            beep_score()

    def drop(self):
        if self.can_place(self.px, self.py + 1, self.ptype, self.prot):
            self.py += 1
        else:
            self.lock()
            self.clear_lines()
            self.spawn()

    def init(self):
        self.board = [[0]*8 for _ in range(8)]
        self.drop_interval = 640
        self.last_drop = time.ticks_ms()
        self.soft_drop_time = time.ticks_ms()
        self.spawn()

    def update(self, now):
        if take_press(B_LEFT) and self.can_place(self.px - 1, self.py, self.ptype, self.prot):
            self.px -= 1; beep_button()
        if take_press(B_RIGHT) and self.can_place(self.px + 1, self.py, self.ptype, self.prot):
            self.px += 1; beep_button()
        if take_press(B_PAUSE):
            nr = (self.prot + 1) % 4
            if self.can_place(self.px, self.py, self.ptype, nr):
                self.prot = nr; beep_button()
        
        if take_press(B_DOWN) or (is_down(B_DOWN) and time.ticks_diff(now, self.soft_drop_time) >= 90):
            self.soft_drop_time = now
            self.drop()

        if time.ticks_diff(now, self.last_drop) >= self.drop_interval:
            self.last_drop = now
            self.drop()

        self.drop_interval = max(120, 640 - score * 3)

    def draw(self):
        clear_frame()
        colors = [0, 0x000022, 0x221100, 0x002200, 0x220000, 0x001122]
        for y in range(8):
            for x in range(8):
                if self.board[y][x]:
                    c = colors[self.board[y][x]]
                    set_pixel(x, y, ((c>>16)&0xFF, (c>>8)&0xFF, c&0xFF))
        
        shape = TETRIS_SHAPES[self.ptype][self.prot]
        for i in range(4):
            set_pixel(self.px + shape[0][i], self.py + shape[1][i], (60, 60, 60))

# --- Flappy ---
class FlappyGame(Game):
    def __init__(self, hard):
        self.hard = hard
        self.y8 = 0
        self.vel8 = 0
        self.pipe_x = 0
        self.gap_y = 0
        self.scored = False
        self.move_interval = 0
        self.last_move = 0
        # Easy mode specific
        self.bird_y = 0
        self.vel = 0
        self.gap_size = 3

    def new_pipe(self):
        self.pipe_x = 8 if self.hard else 7
        self.gap_y = random.randint(1, 4)
        self.scored = False

    def init(self):
        if self.hard:
            self.y8 = 28
            self.vel8 = 0
            self.move_interval = 125
        else:
            self.bird_y = 4
            self.vel = 0
            self.move_interval = 200
        self.last_move = time.ticks_ms()
        self.new_pipe()

    def update(self, now):
        global score
        if take_press(B_UP) or take_press(B_PAUSE):
            if self.hard: self.vel8 = -14
            else: self.vel = -2
            beep_button()

        if time.ticks_diff(now, self.last_move) < self.move_interval: return
        self.last_move = now

        if self.hard:
            self.vel8 += 4
            if self.vel8 > 18: self.vel8 = 18
            self.y8 += self.vel8
            bird_y = self.y8 // 8
            
            self.pipe_x -= 1
            if self.pipe_x < -2: self.new_pipe()
            
            if not self.scored and self.pipe_x + 1 < 2:
                self.scored = True
                score += 1
                beep_score()
            
            if self.y8 < 0 or bird_y > 7:
                beep_hit(); finish_game(False, "Crashed"); return
            
            if 2 >= self.pipe_x and 2 < self.pipe_x + 2:
                if bird_y < self.gap_y or bird_y >= self.gap_y + 3:
                    beep_hit(); finish_game(False, "Hit pipe"); return
            
            self.move_interval = max(65, 125 - score * 2)
        else:
            self.vel += 1
            self.bird_y += self.vel
            if self.bird_y < 0 or self.bird_y > 7:
                beep_hit(); finish_game(False, "Crashed"); return
            
            self.pipe_x -= 1
            if self.pipe_x < 0:
                self.new_pipe()
                score += 10
                beep_score()
            
            if self.pipe_x == 0:
                if self.bird_y < self.gap_y or self.bird_y >= self.gap_y + self.gap_size:
                    beep_hit(); finish_game(False, "Hit pipe"); return

    def draw(self):
        clear_frame()
        if self.hard:
            for x in range(self.pipe_x, self.pipe_x + 2):
                for y in range(8):
                    if y < self.gap_y or y >= self.gap_y + 3:
                        set_pixel(x, y, (0, 35, 8))
            set_pixel(2, self.y8 // 8, (70, 55, 0))
        else:
            set_pixel(0, self.bird_y, (70, 55, 0))
            for y in range(8):
                if y < self.gap_y or y >= self.gap_y + self.gap_size:
                    set_pixel(self.pipe_x, y, (0, 55, 0))

# --- Asteroids ---
class AsteroidsGame(Game):
    def __init__(self):
        self.ship_x = 3
        self.bullets = [] # (x, y)
        self.rocks = [] # (x, y, interval, last_move)
        self.spawn_interval = 900
        self.last_move = 0
        self.last_bullet = 0
        self.last_spawn = 0
        self.last_shot = 0

    def init(self):
        self.ship_x = 3
        self.bullets = []
        self.rocks = []
        self.spawn_interval = 900
        now = time.ticks_ms()
        self.last_move = now
        self.last_bullet = now
        self.last_spawn = now
        self.last_shot = now

    def update(self, now):
        global score
        if is_down(B_LEFT) and time.ticks_diff(now, self.last_move) >= 90:
            self.last_move = now
            if self.ship_x > 0: self.ship_x -= 1; beep_button()
        if is_down(B_RIGHT) and time.ticks_diff(now, self.last_move) >= 90:
            self.last_move = now
            if self.ship_x < 7: self.ship_x += 1; beep_button()
        
        if (take_press(B_UP) or take_press(B_PAUSE)) and time.ticks_diff(now, self.last_shot) >= 170:
            self.last_shot = now
            if len(self.bullets) < 5:
                self.bullets.append([self.ship_x, 6])
                beep_button()

        if time.ticks_diff(now, self.last_bullet) >= 85:
            self.last_bullet = now
            for b in self.bullets: b[1] -= 1
            self.bullets = [b for b in self.bullets if b[1] >= 0]

        if time.ticks_diff(now, self.last_spawn) >= self.spawn_interval:
            self.last_spawn = now
            if len(self.rocks) < 8:
                interval = max(90, random.randint(180, 320) - score * 2)
                self.rocks.append([random.randint(0, 7), 0, interval, now])

        for r in self.rocks:
            if time.ticks_diff(now, r[3]) >= r[2]:
                r[3] = now
                r[1] += 1
        self.rocks = [r for r in self.rocks if r[1] <= 7]

        # Collisions
        for b in self.bullets[:]:
            hit = False
            for r in self.rocks[:]:
                if b[0] == r[0] and b[1] == r[1]:
                    self.rocks.remove(r)
                    hit = True
                    score += 1
                    beep_score()
                    break
            if hit: self.bullets.remove(b)

        for r in self.rocks:
            if r[0] == self.ship_x and r[1] == 7:
                beep_hit()
                finish_game(False, "Ship hit")
                return

        self.spawn_interval = max(220, 900 - score * 16)

# --- Pacman ---
PACMAN_WORLDS = [
    [0x00, 0x7E, 0x52, 0x7A, 0x5E, 0x62, 0x7E, 0x00],
    [0x00, 0x7E, 0x56, 0x76, 0x5C, 0x6E, 0x7A, 0x00],
    [0x00, 0x76, 0x5C, 0x6A, 0x3E, 0x56, 0x7E, 0x00],
    [0x00, 0x7E, 0x52, 0x7E, 0x54, 0x7E, 0x6A, 0x00]
]

class PacmanGame(Game):
    def __init__(self, hard):
        self.hard = hard
        self.px = 0
        self.py = 0
        self.gx = 0
        self.gy = 0
        self.dx = 0
        self.dy = 0
        self.next_dx = 0
        self.next_dy = 0
        self.pellets = [[0]*8 for _ in range(8)]
        self.pellets_left = 0
        self.last_pac = 0
        self.last_ghost = 0
        self.world_idx = 0
        self.pac_int = 0
        self.ghost_int = 0
        self.cells = [[0]*8 for _ in range(8)] # 0=wall, 1=pellet, 2=empty

    def load_world(self):
        self.pellets_left = 0
        if self.hard:
            map_data = PACMAN_WORLDS[self.world_idx]
            for y in range(8):
                row = map_data[y]
                for x in range(8):
                    if (row >> (7-x)) & 1:
                        self.cells[y][x] = 1
                        self.pellets_left += 1
                    else:
                        self.cells[y][x] = 0
            # Clear start pos
            if self.cells[self.py][self.px] == 1:
                self.cells[self.py][self.px] = 2
                self.pellets_left -= 1
            if self.cells[self.gy][self.gx] == 1:
                self.cells[self.gy][self.gx] = 2
                self.pellets_left -= 1
        else:
            for y in range(8):
                for x in range(8):
                    self.pellets[y][x] = 1
                    self.pellets_left += 1
            self.pellets[self.py][self.px] = 0
            self.pellets_left -= 1

    def init(self):
        self.px, self.py = 4, 4 if not self.hard else 1
        if self.hard: self.px = 1
        self.gx, self.gy = 0, 0 if not self.hard else 6
        if self.hard: self.gx = 6
        self.dx, self.dy = 1, 0
        self.next_dx, self.next_dy = 1, 0
        self.world_idx = 0
        self.pac_int = 210 if self.hard else 250
        self.ghost_int = 360 if self.hard else 400
        self.last_pac = time.ticks_ms()
        self.last_ghost = time.ticks_ms()
        self.load_world()

    def is_wall(self, x, y):
        if x < 0 or x > 7 or y < 0 or y > 7: return True
        if self.hard: return self.cells[y][x] == 0
        return False

    def update(self, now):
        global score, game_paused
        if take_press(B_PAUSE):
            game_paused = not game_paused
            beep_button()
        if game_paused: return

        if take_press(B_UP): self.next_dx, self.next_dy = 0, -1; beep_button()
        elif take_press(B_DOWN): self.next_dx, self.next_dy = 0, 1; beep_button()
        elif take_press(B_LEFT): self.next_dx, self.next_dy = -1, 0; beep_button()
        elif take_press(B_RIGHT): self.next_dx, self.next_dy = 1, 0; beep_button()

        if time.ticks_diff(now, self.last_pac) >= self.pac_int:
            self.last_pac = now
            if not self.hard:
                # Easy mode movement
                nx, ny = self.px + self.next_dx, self.py + self.next_dy
                self.px = (nx + 8) % 8
                self.py = (ny + 8) % 8
                if self.pellets[self.py][self.px]:
                    self.pellets[self.py][self.px] = 0
                    self.pellets_left -= 1
                    score += 10
                    beep_score()
                    if self.pellets_left == 0:
                        score += 100
                        self.load_world()
            else:
                # Hard mode
                if not self.is_wall(self.px + self.next_dx, self.py + self.next_dy):
                    self.dx, self.dy = self.next_dx, self.next_dy
                nx, ny = self.px + self.dx, self.py + self.dy
                if not self.is_wall(nx, ny):
                    self.px, self.py = nx, ny
                if self.cells[self.py][self.px] == 1:
                    self.cells[self.py][self.px] = 2
                    self.pellets_left -= 1
                    score += 1
                    beep_score()

        if time.ticks_diff(now, self.last_ghost) >= self.ghost_int:
            self.last_ghost = now
            if not self.hard:
                if abs(self.gx - self.px) > abs(self.gy - self.py):
                    self.gx += 1 if self.gx < self.px else -1
                else:
                    self.gy += 1 if self.gy < self.py else -1
                self.gx %= 8; self.gy %= 8
            else:
                # Hard AI
                best_d = 999
                bx, by = self.gx, self.gy
                dirs = [(0,1),(0,-1),(1,0),(-1,0)]
                for d in dirs:
                    nx, ny = self.gx + d[0], self.gy + d[1]
                    if self.is_wall(nx, ny): continue
                    dist = abs(nx - self.px) + abs(ny - self.py)
                    if dist < best_d or (dist == best_d and random.randint(0,1)):
                        best_d = dist
                        bx, by = nx, ny
                self.gx, self.gy = bx, by

        if self.px == self.gx and self.py == self.gy:
            beep_hit()
            finish_game(False, "Caught")
            return

        if self.hard and self.pellets_left == 0:
            if self.world_idx < 3:
                self.world_idx += 1
                score += 25
                beep_score()
                self.px, self.py = 1, 1
                self.gx, self.gy = 6, 6
                self.dx, self.dy = 1, 0
                self.next_dx, self.next_dy = 1, 0
                self.ghost_int = max(120, self.ghost_int - 18)
                self.pac_int = max(145, self.pac_int - 10)
                self.load_world()
            else:
                score += 100
                finish_game(True, "World 4 Clear")

    def draw(self):
        clear_frame()
        if self.hard:
            for y in range(8):
                for x in range(8):
                    if self.cells[y][x] == 0: set_pixel(x, y, (0, 0, 24))
                    elif self.cells[y][x] == 1: set_pixel(x, y, (170, 150, 55))
        else:
            for y in range(8):
                for x in range(8):
                    if self.pellets[y][x]: set_pixel(x, y, (0, 28, 45))
        set_pixel(self.gx, self.gy, (45, 0, 0))
        set_pixel(self.px, self.py, (255, 95, 0))

# --- Space Shooter ---
class SpaceShooterGame(Game):
    def __init__(self):
        self.ship_x = 3
        self.p_bullets = []
        self.e_bullets = []
        self.enemies = []
        self.e_int = 720
        self.s_int = 1350
        self.last_move = 0
        self.last_pb = 0
        self.last_eb = 0
        self.last_estep = 0
        self.last_spawn = 0
        self.last_shot = 0
        self.last_eshot = 0

    def init(self):
        self.ship_x = 3
        self.p_bullets = []
        self.e_bullets = []
        self.enemies = []
        self.e_int = 720
        self.s_int = 1350
        now = time.ticks_ms()
        self.last_move = now
        self.last_pb = now
        self.last_eb = now
        self.last_estep = now
        self.last_spawn = now
        self.last_shot = now
        self.last_eshot = now

    def update(self, now):
        global score
        if is_down(B_LEFT) and time.ticks_diff(now, self.last_move) >= 125:
            self.last_move = now
            if self.ship_x > 0: self.ship_x -= 1; beep_button()
        if is_down(B_RIGHT) and time.ticks_diff(now, self.last_move) >= 125:
            self.last_move = now
            if self.ship_x < 7: self.ship_x += 1; beep_button()

        if (take_press(B_UP) or take_press(B_PAUSE)) and time.ticks_diff(now, self.last_shot) >= 320:
            self.last_shot = now
            if len(self.p_bullets) < 6: self.p_bullets.append([self.ship_x, 6]); beep_button()

        if time.ticks_diff(now, self.last_spawn) >= self.s_int:
            self.last_spawn = now
            if len(self.enemies) < 8: self.enemies.append([random.randint(0, 7), 0])

        if time.ticks_diff(now, self.last_pb) >= 130:
            self.last_pb = now
            for b in self.p_bullets: b[1] -= 1
            self.p_bullets = [b for b in self.p_bullets if b[1] >= 0]

        if time.ticks_diff(now, self.last_estep) >= self.e_int:
            self.last_estep = now
            for e in self.enemies:
                if random.randint(0, 3) == 0:
                    e[0] = max(0, min(7, e[0] + random.randint(-1, 1)))
                e[1] += 1
                if e[1] >= 7: beep_hit(); finish_game(False, "Line broken"); return

        if time.ticks_diff(now, self.last_eshot) >= 850:
            self.last_eshot = now
            if self.enemies:
                e = self.enemies[random.randint(0, len(self.enemies)-1)]
                if len(self.e_bullets) < 5: self.e_bullets.append([e[0], e[1]+1])

        if time.ticks_diff(now, self.last_eb) >= 190:
            self.last_eb = now
            for b in self.e_bullets:
                b[1] += 1
                if b[1] == 7 and b[0] == self.ship_x:
                    beep_hit(); finish_game(False, "Shot down"); return
            self.e_bullets = [b for b in self.e_bullets if b[1] <= 7]

        for b in self.p_bullets[:]:
            hit = False
            for e in self.enemies[:]:
                if b[0] == e[0] and b[1] == e[1]:
                    self.enemies.remove(e)
                    hit = True
                    score += 1
                    beep_score()
                    break
            if hit: self.p_bullets.remove(b)

        self.e_int = max(300, 720 - score * 2)
        self.s_int = max(500, 1350 - score * 5)

    def draw(self):
        clear_frame()
        for e in self.enemies: set_pixel(e[0], e[1], (46, 0, 20))
        for b in self.p_bullets: set_pixel(b[0], b[1], (65, 65, 65))
        for b in self.e_bullets: set_pixel(b[0], b[1], (55, 10, 0))
        set_pixel(self.ship_x, 7, (0, 60, 10))

# --- Breakout ---
class BreakoutGame(Game):
    def __init__(self):
        self.bricks = [[True]*8 for _ in range(3)]
        self.px = 2
        self.bx = 3
        self.by = 6
        self.vx = 1
        self.vy = -1
        self.move_int = 200
        self.last_move = 0
        self.last_pmove = 0

    def init(self):
        self.bricks = [[True]*8 for _ in range(3)]
        self.px = 2
        self.bx, self.by = 3, 6
        self.vx, self.vy = (1 if random.randint(0,1) else -1), -1
        self.move_int = 200
        self.last_move = time.ticks_ms()
        self.last_pmove = time.ticks_ms()

    def update(self, now):
        global score, game_paused
        if take_press(B_PAUSE):
            game_paused = not game_paused
            beep_button()
        if game_paused: return

        if is_down(B_LEFT) and time.ticks_diff(now, self.last_pmove) >= 80:
            self.last_pmove = now
            if self.px > 0: self.px -= 1; beep_button()
        if is_down(B_RIGHT) and time.ticks_diff(now, self.last_pmove) >= 80:
            self.last_pmove = now
            if self.px < 5: self.px += 1; beep_button()

        if time.ticks_diff(now, self.last_move) < self.move_int: return
        self.last_move = now

        nx, ny = self.bx + self.vx, self.by + self.vy
        if nx < 0 or nx > 7:
            self.vx = -self.vx
            nx = self.bx + self.vx
        if ny < 0:
            self.vy = 1
            ny = self.by + self.vy
        
        if 0 <= ny < 3 and self.bricks[ny][nx]:
            self.bricks[ny][nx] = False
            self.vy = -self.vy
            ny = self.by + self.vy
            score += 1
            beep_score()

        if ny >= 7:
            if self.px <= nx < self.px + 3:
                self.vy = -1
                ny = 6
                hit = nx - (self.px + 1)
                if hit != 0: self.vx = hit
                beep_button()
            else:
                beep_hit()
                finish_game(False, "Missed ball")
                return

        self.bx, self.by = nx, ny
        if not any(any(row) for row in self.bricks):
            score += 5
            beep_win()
            self.bricks = [[True]*8 for _ in range(3)]
            self.bx, self.by = 3, 6
            self.move_int = max(80, self.move_int - 12)

    def draw(self):
        clear_frame()
        for y in range(3):
            for x in range(8):
                if self.bricks[y][x]:
                    set_pixel(x, y, (35+y*6, 10+y*4, 0))
        for x in range(3):
            set_pixel(self.px + x, 7, (0, 48, 50))
        set_pixel(self.bx, self.by, (65, 65, 65))

# --- TicTacToe ---
class TTTGame(Game):
    def __init__(self, ai):
        self.ai = ai
        self.board = [[0]*3 for _ in range(3)]
        self.cx = 1
        self.cy = 1
        self.turn = 1
        self.waiting_ai = False
        self.ai_time = 0

    def init(self):
        self.board = [[0]*3 for _ in range(3)]
        self.cx, self.cy = 1, 1
        self.turn = 1
        self.waiting_ai = False

    def check_win(self):
        b = self.board
        for i in range(3):
            if b[i][0] and b[i][0] == b[i][1] == b[i][2]: return b[i][0]
            if b[0][i] and b[0][i] == b[1][i] == b[2][i]: return b[0][i]
        if b[0][0] and b[0][0] == b[1][1] == b[2][2]: return b[0][0]
        if b[0][2] and b[0][2] == b[1][1] == b[2][0]: return b[0][2]
        if all(all(row) for row in b): return 3 # Draw
        return 0

    def ai_move(self):
        # Simple AI: Win -> Block -> Center -> Random
        for p in [2, 1]:
            for y in range(3):
                for x in range(3):
                    if not self.board[y][x]:
                        self.board[y][x] = p
                        if self.check_win() == p:
                            self.board[y][x] = 2
                            return
                        self.board[y][x] = 0
        if not self.board[1][1]: self.board[1][1] = 2; return
        for y in range(3):
            for x in range(3):
                if not self.board[y][x]: self.board[y][x] = 2; return

    def update(self, now):
        global score
        if self.ai and self.waiting_ai:
            if now >= self.ai_time:
                self.waiting_ai = False
                self.ai_move()
                res = self.check_win()
                if res:
                    if res == 1: score += 5; finish_game(True, "You Win")
                    elif res == 2: finish_game(False, "AI Wins")
                    else: finish_game(False, "Draw")
                self.turn = 1
            return

        if take_press(B_UP) and self.cy > 0: self.cy -= 1; beep_button()
        if take_press(B_DOWN) and self.cy < 2: self.cy += 1; beep_button()
        if take_press(B_LEFT) and self.cx > 0: self.cx -= 1; beep_button()
        if take_press(B_RIGHT) and self.cx < 2: self.cx += 1; beep_button()

        if take_press(B_PAUSE):
            if not self.board[self.cy][self.cx]:
                self.board[self.cy][self.cx] = self.turn
                score += 1
                beep_button()
                res = self.check_win()
                if res:
                    if res == 1: score += 5; finish_game(True, "P1 Wins")
                    elif res == 2: finish_game(True, "P2 Wins")
                    else: finish_game(False, "Draw")
                    return
                
                if self.ai:
                    self.turn = 2
                    self.waiting_ai = True
                    self.ai_time = now + 500
                else:
                    self.turn = 3 - self.turn
            else:
                beep_hit()

    def draw(self):
        clear_frame()
        # Grid
        for i in range(8):
            set_pixel(2, i, (8,8,8)); set_pixel(5, i, (8,8,8))
            set_pixel(i, 2, (8,8,8)); set_pixel(i, 5, (8,8,8))
        
        coords = [1, 3, 6]
        for y in range(3):
            for x in range(3):
                gx, gy = coords[x], coords[y]
                p = self.board[y][x]
                if p == 1: set_pixel(gx, gy, (0, 52, 0))
                elif p == 2: set_pixel(gx, gy, (55, 0, 0))
                
                if x == self.cx and y == self.cy and (not self.ai or self.turn == 1):
                    set_pixel(gx, gy, (65, 65, 65) if p else (0, 0, 65))

# --- Pong ---
class PongGame(Game):
    def __init__(self):
        self.px = 3
        self.ax = 3
        self.bx = 4
        self.by = 4
        self.vx = 1
        self.vy = 1
        self.move_int = 155
        self.last_move = 0
        self.last_pmove = 0
        self.last_amove = 0

    def init(self):
        self.px, self.ax = 3, 3
        self.bx, self.by = 4, 4
        self.vx, self.vy = (1 if random.randint(0,1) else -1), 1
        self.move_int = 155
        self.last_move = time.ticks_ms()
        self.last_pmove = time.ticks_ms()
        self.last_amove = time.ticks_ms()

    def update(self, now):
        global score, game_paused
        if take_press(B_PAUSE):
            game_paused = not game_paused
            beep_button()
        if game_paused: return

        if is_down(B_LEFT) and time.ticks_diff(now, self.last_pmove) >= 60:
            self.last_pmove = now
            if self.px > 0: self.px -= 1
        if is_down(B_RIGHT) and time.ticks_diff(now, self.last_pmove) >= 60:
            self.last_pmove = now
            if self.px < 5: self.px += 1

        if time.ticks_diff(now, self.last_amove) >= 110:
            self.last_amove = now
            tx = self.bx - 1
            if self.ax < tx and self.ax < 5: self.ax += 1
            elif self.ax > tx and self.ax > 0: self.ax -= 1

        if time.ticks_diff(now, self.last_move) < self.move_int: return
        self.last_move = now

        nx, ny = self.bx + self.vx, self.by + self.vy
        if nx < 0: nx, self.vx = 0, 1
        elif nx > 7: nx, self.vx = 7, -1

        if ny >= 7:
            if self.px <= nx < self.px + 3:
                ny = 6
                self.vy = -1
                hit = nx - (self.px + 1)
                if hit: self.vx = hit
                score += 1
                beep_button()
            else:
                beep_hit()
                finish_game(False, "Missed")
                return
        elif ny <= 0:
            if self.ax <= nx < self.ax + 3:
                ny = 1
                self.vy = 1
                hit = nx - (self.ax + 1)
                if hit: self.vx = hit
                beep_button()
            else:
                score += 20
                beep_score()
                self.bx, self.by = 4, 4
                self.vy = 1
                self.move_int = max(50, self.move_int - 1)
                return

        self.bx, self.by = nx, ny

    def draw(self):
        clear_frame()
        for i in range(3):
            set_pixel(self.px + i, 7, (0, 80, 0))
            set_pixel(self.ax + i, 0, (80, 0, 0))
        set_pixel(self.bx, self.by, (80, 80, 80))

# --- Tug ---
class TugGame(Game):
    def __init__(self):
        self.pos = 4
        self.p1_last = 0
        self.p2_last = 0

    def init(self):
        self.pos = 4
        self.p1_last = time.ticks_ms()
        self.p2_last = time.ticks_ms()

    def update(self, now):
        global score
        if (take_press(B_UP) or take_press(B_LEFT)) and time.ticks_diff(now, self.p1_last) >= 80:
            self.p1_last = now
            self.pos -= 1
            score += 1
            beep_button()
        if (take_press(B_DOWN) or take_press(B_RIGHT)) and time.ticks_diff(now, self.p2_last) >= 80:
            self.p2_last = now
            self.pos += 1
            score += 1
            beep_button()
        
        if self.pos <= 0: finish_game(True, "P1 Wins")
        elif self.pos >= 7: finish_game(True, "P2 Wins")

    def draw(self):
        clear_frame()
        for x in range(8): set_pixel(x, 3, (8,8,8))
        set_pixel(0, 3, (0, 50, 0))
        set_pixel(7, 3, (50, 0, 0))
        set_pixel(self.pos, 3, (65, 55, 0))

# --- Checkers ---
class CheckersGame(Game):
    def __init__(self, ai):
        self.ai = ai
        self.board = [[0]*8 for _ in range(8)]
        self.cx, self.cy = 0, 5
        self.sx, self.sy = -1, -1
        self.turn = 1
        self.valid_moves = []
        self.waiting_ai = False
        self.ai_time = 0
        self.ai_blink = 0
        self.ai_last_move = None

    def init(self):
        self.board = [[0]*8 for _ in range(8)]
        for y in range(3):
            for x in range(8):
                if (x+y)%2: self.board[y][x] = 3
        for y in range(5, 8):
            for x in range(8):
                if (x+y)%2: self.board[y][x] = 1
        self.cx, self.cy = 0, 5
        self.sx, self.sy = -1, -1
        self.turn = 1
        self.waiting_ai = False
        self.ai_last_move = None

    def get_moves(self, x, y):
        p = self.board[y][x]
        if not p: return []
        is_p1 = (p in [1, 2])
        is_king = (p in [2, 4])
        moves = []
        
        # Jumps
        dirs = [(-1,-1), (1,-1), (-1,1), (1,1)]
        for dx, dy in dirs:
            if not is_king:
                if is_p1 and dy > 0: continue
                if not is_p1 and dy < 0: continue
            
            # Check jump
            mx, my = x + dx, y + dy
            jx, jy = x + dx*2, y + dy*2
            if 0 <= jx < 8 and 0 <= jy < 8:
                mid = self.board[my][mx]
                if mid and ((is_p1 and mid in [3,4]) or (not is_p1 and mid in [1,2])):
                    if self.board[jy][jx] == 0:
                        moves.append((jx, jy, True))
        
        if moves: return moves # Forced capture

        # Regular moves
        for dx, dy in dirs:
            if not is_king:
                if is_p1 and dy > 0: continue
                if not is_p1 and dy < 0: continue
            nx, ny = x + dx, y + dy
            if 0 <= nx < 8 and 0 <= ny < 8 and self.board[ny][nx] == 0:
                moves.append((nx, ny, False))
        return moves

    def execute(self, fx, fy, tx, ty):
        global score
        p = self.board[fy][fx]
        self.board[ty][tx] = p
        self.board[fy][fx] = 0
        
        if abs(tx - fx) == 2:
            mx, my = (fx+tx)//2, (fy+ty)//2
            self.board[my][mx] = 0
            score += 10
            beep_score()
        
        if p == 1 and ty == 0: self.board[ty][tx] = 2; beep_win()
        if p == 3 and ty == 7: self.board[ty][tx] = 4; beep_win()
        
        self.turn = 3 - self.turn
        self.sx, self.sy = -1, -1
        self.valid_moves = []

    def ai_move(self):
        best_score = -9999
        best_move = None
        
        for y in range(8):
            for x in range(8):
                if self.board[y][x] in [3, 4]:
                    moves = self.get_moves(x, y)
                    for mx, my, capture in moves:
                        s = 0
                        if capture: s += 100
                        if my == 7: s += 50
                        s += my * 5
                        s += random.randint(0, 5)
                        if s > best_score:
                            best_score = s
                            best_move = (x, y, mx, my)
        
        if best_move:
            self.execute(*best_move)
            self.ai_last_move = best_move
            self.ai_blink = time.ticks_ms() + 1000
        else:
            finish_game(True, "P1 Wins")

    def update(self, now):
        if self.ai and self.turn == 2:
            if not self.waiting_ai:
                self.waiting_ai = True
                self.ai_time = now + 500
            elif now >= self.ai_time:
                self.waiting_ai = False
                self.ai_move()
            return

        if take_press(B_UP) and self.cy > 0: self.cy -= 1; beep_button()
        if take_press(B_DOWN) and self.cy < 7: self.cy += 1; beep_button()
        if take_press(B_LEFT) and self.cx > 0: self.cx -= 1; beep_button()
        if take_press(B_RIGHT) and self.cx < 7: self.cx += 1; beep_button()

        if take_press(B_PAUSE):
            if self.sx == -1:
                p = self.board[self.cy][self.cx]
                if (self.turn == 1 and p in [1,2]) or (self.turn == 2 and p in [3,4]):
                    self.sx, self.sy = self.cx, self.cy
                    self.valid_moves = self.get_moves(self.sx, self.sy)
                    if not self.valid_moves: self.sx = -1; beep_hit()
                    else: beep_button()
                else: beep_hit()
            else:
                if self.cx == self.sx and self.cy == self.sy:
                    self.sx = -1; self.valid_moves = []; beep_button()
                else:
                    valid = False
                    for m in self.valid_moves:
                        if m[0] == self.cx and m[1] == self.cy:
                            self.execute(self.sx, self.sy, self.cx, self.cy)
                            valid = True
                            break
                    if not valid: beep_hit()

    def draw(self):
        clear_frame()
        for y in range(8):
            for x in range(8):
                if (x+y)%2:
                    c = (15,15,15)
                    p = self.board[y][x]
                    if p == 1: c = (0,70,0)
                    elif p == 2: c = (50,200,50)
                    elif p == 3: c = (70,0,0)
                    elif p == 4: c = (200,50,50)
                    set_pixel(x, y, c)
                else:
                    set_pixel(x, y, (5,5,5))
        
        if self.sx != -1: set_pixel(self.sx, self.sy, (100,100,0))
        for m in self.valid_moves: set_pixel(m[0], m[1], (50,50,0))
        
        if not self.ai or self.turn == 1:
            if (time.ticks_ms() // 300) % 2:
                set_pixel(self.cx, self.cy, (80,80,80))
        
        if self.ai and self.waiting_ai and (time.ticks_ms() // 200) % 2:
            for i in range(8):
                set_pixel(i, 0, (40,0,0)); set_pixel(i, 7, (40,0,0))

# --- Minesweeper ---
class MinesweeperGame(Game):
    def __init__(self):
        self.board = [[0]*8 for _ in range(8)] # 9=mine, 0-8=count
        self.revealed = [[0]*8 for _ in range(8)] # 0=hidden, 1=rev, 2=flag
        self.cx, self.cy = 4, 4
        self.first = True
        self.to_reveal = 0
        self.pause_start = 0
        self.pause_held = False

    def place_mines(self, safe_x, safe_y):
        count = 0
        while count < 10:
            x, y = random.randint(0, 7), random.randint(0, 7)
            if self.board[y][x] != 9 and (x != safe_x or y != safe_y):
                self.board[y][x] = 9
                count += 1
        
        for y in range(8):
            for x in range(8):
                if self.board[y][x] == 9: continue
                c = 0
                for dy in [-1,0,1]:
                    for dx in [-1,0,1]:
                        if 0 <= x+dx < 8 and 0 <= y+dy < 8 and self.board[y+dy][x+dx] == 9:
                            c += 1
                self.board[y][x] = c

    def flood(self, x, y):
        if not (0 <= x < 8 and 0 <= y < 8) or self.revealed[y][x]: return
        self.revealed[y][x] = 1
        self.to_reveal -= 1
        global score; score += 1
        if self.board[y][x] == 0:
            for dy in [-1,0,1]:
                for dx in [-1,0,1]:
                    self.flood(x+dx, y+dy)

    def init(self):
        self.board = [[0]*8 for _ in range(8)]
        self.revealed = [[0]*8 for _ in range(8)]
        self.cx, self.cy = 4, 4
        self.first = True
        self.to_reveal = 64 - 10
        self.pause_start = 0
        self.pause_held = False

    def update(self, now):
        if take_press(B_UP): self.cy = (self.cy - 1) % 8; beep_button()
        if take_press(B_DOWN): self.cy = (self.cy + 1) % 8; beep_button()
        if take_press(B_LEFT): self.cx = (self.cx - 1) % 8; beep_button()
        if take_press(B_RIGHT): self.cx = (self.cx + 1) % 8; beep_button()

        if is_down(B_PAUSE):
            if not self.pause_held:
                self.pause_start = now
                self.pause_held = True
            elif now - self.pause_start >= 500: # Long press
                if self.revealed[self.cy][self.cx] == 0:
                    if self.first:
                        self.place_mines(self.cx, self.cy)
                        self.first = False
                    
                    if self.board[self.cy][self.cx] == 9:
                        for y in range(8):
                            for x in range(8):
                                if self.board[y][x] == 9: self.revealed[y][x] = 1
                        finish_game(False, "Boom")
                    else:
                        beep_score()
                        self.flood(self.cx, self.cy)
                        if self.to_reveal == 0: finish_game(True, "Cleared")
                self.pause_held = False # Consume
                self.pause_start = now + 99999
        elif self.pause_held: # Released short
            if now - self.pause_start < 500:
                s = self.revealed[self.cy][self.cx]
                if s == 0: self.revealed[self.cy][self.cx] = 2; beep_button()
                elif s == 2: self.revealed[self.cy][self.cx] = 0; beep_button()
            self.pause_held = False

    def draw(self):
        clear_frame()
        for y in range(8):
            for x in range(8):
                s = self.revealed[y][x]
                if s == 0: set_pixel(x, y, (15,15,15))
                elif s == 2: set_pixel(x, y, (0,0,90))
                else:
                    v = self.board[y][x]
                    if v == 9: set_pixel(x, y, (100,0,0))
                    elif v == 0: set_pixel(x, y, (40,0,40))
                    elif v == 1: set_pixel(x, y, (0,50,0))
                    elif v == 2: set_pixel(x, y, (50,50,0))
                    else: set_pixel(x, y, (v*15,0,0))
        
        if (time.ticks_ms() // 250) % 2:
            set_pixel(self.cx, self.cy, (100,100,100))

# --- Dino ---
class DinoGame(Game):
    def __init__(self):
        self.y8 = 56
        self.v8 = 0
        self.crouch = False
        self.ox = 0
        self.oy = 0
        self.ow = 0
        self.oh = 0
        self.otype = 0
        self.passed = False
        self.move_int = 150
        self.last_move = 0

    def new_obs(self):
        self.otype = random.randint(0, 1)
        self.ow = random.randint(1, 2)
        self.ox = -self.ow
        if self.otype == 0: # Ground
            self.oh = random.randint(1, 3)
            self.oy = 7
        else:
            self.oh = random.randint(1, 2)
            self.oy = random.randint(4, 5)
        self.passed = False

    def init(self):
        self.y8 = 56
        self.v8 = 0
        self.crouch = False
        self.move_int = 150
        self.last_move = time.ticks_ms()
        self.new_obs()

    def update(self, now):
        global score, game_paused
        if take_press(B_PAUSE): game_paused = not game_paused; beep_button()
        if game_paused: return

        py = self.y8 // 8
        self.crouch = is_down(B_DOWN) and py == 7
        
        if take_press(B_UP) and py == 7 and not self.crouch:
            self.v8 = int(-28.0 * (150.0/self.move_int))
            beep_button()
        
        if take_press(B_DOWN) and py < 7: self.v8 += 30

        if time.ticks_diff(now, self.last_move) < self.move_int: return
        self.last_move = now

        self.v8 += int(6.0 * (150.0/self.move_int))
        if self.v8 > 24: self.v8 = 24
        self.y8 += self.v8
        if self.y8 >= 56: self.y8 = 56; self.v8 = 0

        self.ox += 1
        if not self.passed and self.ox > 6:
            self.passed = True
            score += 1
            beep_score()
            if self.move_int > 60: self.move_int -= 4
        
        if self.ox > 8: self.new_obs()

        # Collision
        py = 7 if self.crouch else self.y8 // 8
        if self.ox <= 6 < self.ox + self.ow:
            top = 8 - self.oh if self.otype == 0 else self.oy
            bot = 7 if self.otype == 0 else self.oy + self.oh - 1
            if py >= top and py <= bot:
                beep_hit()
                finish_game(False, "Ouch")

    def draw(self):
        clear_frame()
        # Ground
        offset = (time.ticks_ms() // 100)
        for i in range(8):
            c = (20,20,20) if ((i - offset) % 4) < 2 else (10,10,10)
            set_pixel(i, 7, c)
        
        # Dino
        py = 7 if self.crouch else self.y8 // 8
        set_pixel(6, py, (0,40,10) if self.crouch else (0,80,20))

        # Obstacle
        for w in range(self.ow):
            for h in range(self.oh):
                px = self.ox + w
                if 0 <= px < 8:
                    py = (7-h) if self.otype == 0 else (self.oy+h)
                    set_pixel(px, py, (80,40,0))

# ========================================
# Main Logic
# ========================================

def init_game(gid):
    global current_game_obj, score, last_win, game_paused
    score = 0
    last_win = False
    game_paused = False
    
    if gid == GAME_SNAKE_WALL: current_game_obj = SnakeGame(False)
    elif gid == GAME_SNAKE_WRAP: current_game_obj = SnakeGame(True)
    elif gid == GAME_TETRIS: current_game_obj = TetrisGame()
    elif gid == GAME_FLAPPY_EASY: current_game_obj = FlappyGame(False)
    elif gid == GAME_FLAPPY_HARD: current_game_obj = FlappyGame(True)
    elif gid == GAME_ASTEROIDS_HARD: current_game_obj = AsteroidsGame()
    elif gid == GAME_PACMAN_EASY: current_game_obj = PacmanGame(False)
    elif gid == GAME_PACMAN_HARD: current_game_obj = PacmanGame(True)
    elif gid == GAME_SPACE_SHOOTER: current_game_obj = SpaceShooterGame()
    elif gid == GAME_BREAKOUT: current_game_obj = BreakoutGame()
    elif gid == GAME_TTT_AI: current_game_obj = TTTGame(True)
    elif gid == GAME_TTT_2P: current_game_obj = TTTGame(False)
    elif gid == GAME_PONG: current_game_obj = PongGame()
    elif gid == GAME_TUG: current_game_obj = TugGame()
    elif gid == GAME_CHECKERS_AI: current_game_obj = CheckersGame(True)
    elif gid == GAME_CHECKERS_2P: current_game_obj = CheckersGame(False)
    elif gid == GAME_MINESWEEPER: current_game_obj = MinesweeperGame()
    elif gid == GAME_DINO: current_game_obj = DinoGame()
    
    if current_game_obj:
        current_game_obj.init()
        beep_start()

def draw_menu(now):
    clear_frame()
    sweep = (now // 140) % 8
    for y in range(8):
        for x in range(8):
            r = (x * 20 + now // 9) & 0x3F
            g = (y * 18 + now // 13) & 0x3F
            b = (x * 9 + y * 11 + now // 17) & 0x3F
            if x == sweep: r = 70
            set_pixel(x, y, (r//2, g//2, b//2))
    
    if oled:
        oled.fill(0)
        oled.text("NB6_Boy", 0, 0, 1)
        oled.text("MUTE" if sound_muted else "SND", 92, 0, 1)
        
        start = max(0, min(menu_index - 2, GAME_COUNT - 5))
        for i in range(5):
            idx = start + i
            if idx >= GAME_COUNT: break
            prefix = "> " if idx == menu_index else "  "
            oled.text(prefix + GAME_NAMES[idx], 0, 14 + i * 10, 1)
        oled.show()

def draw_playing(now):
    if current_game_obj: current_game_obj.draw()
    
    if game_paused and (now // 200) % 2:
        c = (50, 50, 0)
        for i in range(8):
            set_pixel(0, i, c); set_pixel(7, i, c)
            set_pixel(i, 0, c); set_pixel(i, 7, c)

    if oled:
        oled.fill(0)
        oled.text(GAME_NAMES[current_game_id], 0, 0, 1)
        if sound_muted: oled.text("MUTE", 98, 0, 1)
        
        if game_paused:
            oled.text("*** PAUSED ***", 0, 16, 1)
            oled.text("PAUSE to resume", 0, 28, 1)
        else:
            oled.text("Score: " + str(score), 0, 16, 1)
            oled.text("Best: " + str(high_score[current_game_id]), 0, 28, 1)
        
        oled.text("SEL -> Menu", 0, 50, 1)
        oled.show()

def draw_game_over(now):
    clear_frame()
    pat = (now // 240) % 2
    c = (0, 45, 0) if last_win else (55, 0, 0)
    for y in range(8):
        for x in range(8):
            if ((x+y)%2 == 0) == (pat == 0):
                set_pixel(x, y, c)
    
    if oled:
        oled.fill(0)
        oled.text("Round Complete" if last_win else "Game Over", 0, 0, 1)
        oled.text(GAME_NAMES[current_game_id], 0, 12, 1)
        oled.text(last_msg, 0, 24, 1)
        oled.text("Score: " + str(score), 0, 36, 1)
        oled.text("Best: " + str(high_score[current_game_id]), 0, 48, 1)
        oled.show()

def boot_sequence():
    # N
    clear_frame()
    c = (0, 80, 255)
    for y in range(8): set_pixel(0, y, c); set_pixel(7, y, c); set_pixel(y, y, c)
    show_frame(); time.sleep(1.3)
    
    # B
    clear_frame()
    c = (255, 85, 0)
    for y in range(8): set_pixel(0, y, c)
    for x in [0,1,2,3,4,5,6]: set_pixel(x, 0, c); set_pixel(x, 3, c); set_pixel(x, 7, c)
    set_pixel(7, 1, c); set_pixel(7, 2, c); set_pixel(7, 4, c); set_pixel(7, 5, c); set_pixel(7, 6, c)
    show_frame(); time.sleep(1.3)
    
    # 6
    clear_frame()
    c = (80, 255, 80)
    for y in range(8): set_pixel(0, y, c)
    for x in range(8): set_pixel(x, 7, c); set_pixel(x, 3, c); set_pixel(x, 0, c)
    set_pixel(7, 4, c); set_pixel(7, 5, c); set_pixel(7, 6, c)
    show_frame(); time.sleep(1.3)

# ========================================
# Main Loop
# ========================================

def main():
    global system_state, menu_index, current_game_id, sound_muted
    
    boot_sequence()
    
    last_matrix = 0
    last_oled = 0
    
    while True:
        now = time.ticks_ms()
        update_buttons(now)
        
        # Global Select
        if take_press(B_SELECT) and system_state != STATE_MENU:
            beep_button()
            system_state = STATE_MENU
            game_paused = False
            clear_button_edges()

        if system_state == STATE_MENU:
            if is_down(B_LEFT) and is_down(B_RIGHT):
                # Mute toggle combo
                pass # Logic simplified
            
            if take_press(B_UP):
                menu_index = (menu_index - 1) % GAME_COUNT
                beep_button()
            if take_press(B_DOWN):
                menu_index = (menu_index + 1) % GAME_COUNT
                beep_button()
            if take_press(B_PAUSE):
                beep_button()
                current_game_id = menu_index
                system_state = STATE_PLAYING
                init_game(current_game_id)
                clear_button_edges()
                
        elif system_state == STATE_PLAYING:
            if current_game_obj:
                current_game_obj.update(now)
                
        elif system_state == STATE_GAME_OVER:
            if time.ticks_diff(now, game_over_start_time) >= GAME_OVER_DELAY_MS or take_press(B_PAUSE) or take_press(B_SELECT):
                beep_button()
                system_state = STATE_MENU
                clear_button_edges()

        update_sound(now)

        if time.ticks_diff(now, last_matrix) >= MATRIX_FRAME_MS:
            last_matrix = now
            if system_state == STATE_MENU: draw_menu(now)
            elif system_state == STATE_PLAYING: draw_playing(now)
            else: draw_game_over(now)
            show_frame()

if __name__ == "__main__":
    main()