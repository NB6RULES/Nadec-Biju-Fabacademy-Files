# Quick Start Guide

## Setup

1. **Install Prerequisites**:
   - Install VS Code
   - Install PlatformIO extension
   - Install Wokwi Simulator extension
   - Or install CLI tools: `npm install -g wokwi-cli` and `pip install platformio`

2. **Build the project**:
   ```powershell
   pio run
   ```

3. **Test in Wokwi**:
   - **Option A** (VS Code): Press F1 → "Wokwi: Start Simulator"
   - **Option B** (CLI): Run `.\test.ps1`

## What's Included

- ✅ **diagram.json** - Your Wokwi hardware configuration
- ✅ **wokwi.toml** - Simulator settings
- ✅ **platformio.ini** - PlatformIO project configuration
- ✅ **src/main.cpp** - Example test code
- ✅ **test.ps1** - Automated test script for Windows
- ✅ **TESTING_WORKFLOW.md** - Complete documentation
- ✅ **.github/workflows/wokwi-test.yml** - CI/CD workflow

## Hardware Pin Map

| Component | Pin | Description |
|-----------|-----|-------------|
| Button 1 | D3 | Input with pullup |
| Button 2 | D2 | Input with pullup |
| Button 3 | D7 | Input with pullup |
| Button 4 | D9 | Input with pullup |
| Button 5 | D6 | Input with pullup |
| Button 6 | D1 | Input with pullup |
| Buzzer | D0 | Output |
| LED Matrix | D10 | SPI Data |
| OLED SDA | D4 | I2C Data |
| OLED SCL | D5 | I2C Clock |

## Testing Your Code

1. Write your code in `src/main.cpp`
2. Build: `pio run`
3. Test in Wokwi: `.\test.ps1`
4. Check serial output for results

## Next Steps

- See [TESTING_WORKFLOW.md](TESTING_WORKFLOW.md) for detailed documentation
- Modify `src/main.cpp` to implement your project
- Add libraries in `platformio.ini` as needed
- Test interactively in Wokwi or use the automated script

Happy coding! 🚀
