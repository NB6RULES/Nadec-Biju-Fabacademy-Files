@echo off
cd /d "%~dp0"
echo Installing mpremote tool...
pip install mpremote --quiet --upgrade

echo Uploading game files to XIAO RP2040...
echo Ensure the device is connected and NOT open in Thonny.
mpremote cp ssd1306.py :ssd1306.py cp gameboy.py :gameboy.py cp main.py :main.py

if %errorlevel% neq 0 (
    echo Error: Upload failed. Check USB connection.
    pause
    exit /b
)

echo Resetting device to start game...
mpremote soft-reset

echo Done.
pause
