@echo off
echo === Bura Build Script ===
echo.

:: Check Python
python --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python not found. Install Python 3.10+ first.
    pause
    exit /b 1
)

:: Install dependencies
echo [1/3] Installing dependencies...
pip install pyinstaller flask opencv-python numpy pyserial bettercam pystray Pillow --quiet

:: Build exe
echo [2/3] Building Bura.exe...
pyinstaller --onefile --console --name Bura ^
    --add-data "WebGui.py;." ^
    --hidden-import flask ^
    --hidden-import pystray ^
    --hidden-import PIL ^
    --hidden-import bettercam ^
    --hidden-import serial ^
    --hidden-import serial.tools.list_ports ^
    --hidden-import cv2 ^
    --hidden-import numpy ^
    Main.py

if errorlevel 1 (
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

echo [3/3] Done!
echo.
echo  Output: dist\Bura.exe
echo.
pause
