# Xiao ESP32-C6 Testing Workflow with Wokwi

This workflow guide helps you test your Xiao ESP32-C6 codes using Wokwi simulator.

## Hardware Setup Overview

Your `diagram.json` includes:
- **Xiao ESP32-C6** board
- **8x8 LED Matrix** (connected to D10)
- **SSD1306 OLED Display** (I2C: SDA=D4, SCL=D5, Address=0x3c)
- **Buzzer** (connected to D0)
- **6 Pushbuttons** connected to:
  - btn1 → D3
  - btn2 → D2
  - btn3 → D7
  - btn4 → D9
  - btn5 → D6
  - btn6 → D1

## Prerequisites

### Install Required Tools

1. **VS Code** with extensions:
   - PlatformIO IDE
   - Wokwi Simulator

2. **Wokwi CLI** (for automated testing):
   ```bash
   npm install -g wokwi-cli
   ```

3. **PlatformIO CLI** (optional, for command-line builds):
   ```bash
   pip install platformio
   ```

## Local Development Workflow

### Method 1: VS Code with Wokwi Extension

1. **Create/Open Your Project**
   ```bash
   # Initialize PlatformIO project if not already done
   pio project init --board seeed_xiao_esp32c6
   ```

2. **Configure platformio.ini**
   ```ini
   [env:seeed_xiao_esp32c6]
   platform = espressif32
   board = seeed_xiao_esp32c6
   framework = arduino
   
   ; Add any required libraries
   lib_deps = 
       adafruit/Adafruit SSD1306@^2.5.7
       adafruit/Adafruit GFX Library@^1.11.3
       ; Add other libraries as needed
   
   ; Optional: Custom upload speed
   upload_speed = 921600
   monitor_speed = 115200
   ```

3. **Build Your Firmware**
   - Click PlatformIO icon → Build (checkmark icon)
   - Or use command palette: `PlatformIO: Build`
   - Or terminal: `pio run`

4. **Start Wokwi Simulation**
   - Press `F1` → Type "Wokwi: Start Simulator"
   - Or click the Wokwi icon in the status bar
   - The simulator will load your compiled firmware with the hardware from `diagram.json`

5. **Interact with Simulation**
   - Click pushbuttons to test input handling
   - Observe LED matrix patterns
   - Check OLED display output
   - Monitor serial output in the Serial Monitor panel

6. **Debug** (if needed)
   - Set breakpoints in your code
   - Use GDB debugging features in Wokwi

### Method 2: Wokwi CLI (Automated Testing)

1. **Update wokwi.toml** (ensure it points to correct firmware):
   ```toml
   [wokwi]
   version = 1
   firmware = '.pio/build/seeed_xiao_esp32c6/firmware.hex'
   elf = '.pio/build/seeed_xiao_esp32c6/firmware.elf'
   ```

2. **Build and Test Script**
   Create `test.ps1`:
   ```powershell
   # Build firmware
   Write-Host "Building firmware..." -ForegroundColor Cyan
   pio run
   
   if ($LASTEXITCODE -ne 0) {
       Write-Host "Build failed!" -ForegroundColor Red
       exit 1
   }
   
   # Run Wokwi simulation
   Write-Host "Starting Wokwi simulation..." -ForegroundColor Green
   wokwi-cli --timeout 30000 .
   
   if ($LASTEXITCODE -ne 0) {
       Write-Host "Simulation failed!" -ForegroundColor Red
       exit 1
   }
   
   Write-Host "Test completed successfully!" -ForegroundColor Green
   ```

3. **Run Tests**
   ```bash
   .\test.ps1
   ```

## CI/CD Integration (GitHub Actions)

Create `.github/workflows/wokwi-test.yml` for automated testing on every push.

## Testing Checklist

### Component Testing

- [ ] **Pushbuttons**
  - [ ] btn1 (D3) - Press and verify response
  - [ ] btn2 (D2) - Press and verify response
  - [ ] btn3 (D7) - Press and verify response
  - [ ] btn4 (D9) - Press and verify response
  - [ ] btn5 (D6) - Press and verify response
  - [ ] btn6 (D1) - Press and verify response

- [ ] **LED Matrix**
  - [ ] Display test pattern
  - [ ] Verify all 64 LEDs work
  - [ ] Test animations/scrolling

- [ ] **OLED Display**
  - [ ] I2C communication working
  - [ ] Text rendering correctly
  - [ ] Graphics rendering

- [ ] **Buzzer**
  - [ ] Tone generation works
  - [ ] Different frequencies tested
  - [ ] Volume appropriate (0.1 in config)

- [ ] **Serial Output**
  - [ ] Messages appear correctly
  - [ ] Baud rate correct (115200)
  - [ ] Debug information clear

## Common Issues & Solutions

### Issue 1: Firmware Not Loading
**Solution**: Ensure `wokwi.toml` paths are correct:
```toml
firmware = '.pio/build/seeed_xiao_esp32c6/firmware.hex'
elf = '.pio/build/seeed_xiao_esp32c6/firmware.elf'
```

### Issue 2: I2C Display Not Working
**Solution**: Verify I2C address and pins:
- Address: 0x3c (as configured)
- SDA: D4
- SCL: D5

### Issue 3: LED Matrix Not Displaying
**Solution**: Check:
- DIN connected to D10
- Power connected (VDD to 5V, VSS to GND)
- Library compatible with 8x8 matrix

### Issue 4: Buttons Not Responding
**Solution**: 
- Verify pull-up/pull-down resistor configuration in code
- Use `pinMode(pin, INPUT_PULLUP)` if no external resistors

## Example Test Code Structure

```cpp
// Pin definitions
#define BTN1_PIN  3  // D3
#define BTN2_PIN  2  // D2
#define BTN3_PIN  7  // D7
#define BTN4_PIN  9  // D9
#define BTN5_PIN  6  // D6
#define BTN6_PIN  1  // D1
#define BUZZER_PIN 0  // D0
#define MATRIX_DIN 10 // D10
#define OLED_SDA  4  // D4
#define OLED_SCL  5  // D5

void setup() {
  Serial.begin(115200);
  
  // Initialize buttons
  pinMode(BTN1_PIN, INPUT_PULLUP);
  // ... other buttons
  
  // Initialize buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Initialize I2C display
  // Wire.begin(OLED_SDA, OLED_SCL);
  
  // Initialize LED matrix
  // ... matrix initialization
  
  Serial.println("Xiao ESP32-C6 Test Started");
}

void loop() {
  // Test each component
  testButtons();
  testBuzzer();
  testDisplay();
  testMatrix();
}
```

## Quick Start Commands

```bash
# Build project
pio run

# Run in Wokwi (VS Code extension)
# Press F1 → "Wokwi: Start Simulator"

# Run tests with CLI
wokwi-cli .

# Clean and rebuild
pio run --target clean
pio run

# Upload to real hardware
pio run --target upload

# Monitor serial output
pio device monitor
```

## Additional Resources

- [Wokwi Documentation](https://docs.wokwi.com/)
- [ESP32-C6 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/)
- [PlatformIO Documentation](https://docs.platformio.org/)
- [Xiao ESP32-C6 Wiki](https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/)

## Advanced: Automated Test Scripts

You can create automated tests by:
1. Using the Wokwi CLI with timeout parameters
2. Monitoring serial output for expected test results
3. Creating Python/PowerShell scripts to parse results
4. Integrating with test frameworks like Unity or Google Test

---

**Happy Testing!** 🚀
