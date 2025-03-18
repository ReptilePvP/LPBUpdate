# LPBUpdate - Loss Prevention Logging System for M5Stack CoreS3

## Overview
This project implements a loss prevention logging system for retail environments using the M5Stack CoreS3 hardware. The application allows store employees to quickly log theft incidents with details including gender, clothing colors, and stolen items.

## Features
- Intuitive touch-based user interface
- Gender and item selection
- Color selection for clothing (shirts, pants, shoes)
- Local storage on SD card
- WiFi connectivity for time synchronization
- Formatted entry logging with timestamps
- Power management settings
- Date and time configuration
- WiFi network management

## Hardware Requirements
- M5Stack CoreS3
- M5Stack Dual Button & Key Unit
- SD Card for storage

## Software Dependencies
- M5Unified
- M5GFX
- LVGL (v8.4.0)
- ESP32 Arduino Core

## Development
This project is developed using PlatformIO with the Arduino framework for ESP32.

## Usage
1. Power on the device
2. Navigate through the menus to log theft incidents
3. Optionally connect to WiFi for time synchronization
4. View logs through the device interface

## Project Structure
- `src/LBPLAT.ino`: Main application code
- `src/References.md`: Detailed documentation and reference
- `src/WiFiManager.h`: Custom WiFi management class

## License
Proprietary - All rights reserved