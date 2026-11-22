# Pico-Safe-Lockbox
- A keypad-controlled safe I made for my CS 390 Embedded Systems class
- I built it using a Raspberry Pi Pico 2 W, C, a 3x4 keypad, two LEDs, a buzzer, and an OLED display
- The goal is to create a safe that can be locked and unlocked using a personal 6-digit PIN

## How It Works
1. The safe starts unlocked with no user PIN set
2. Press `*` to activate the safe
3. Enter a 6-digit PIN and press `#` to save it
4. The safe locks after the PIN is saved
5. Press `*` and enter the PIN to unlock the safe
6. Press `#` to lock the safe again using the same PIN
7. Press `*` to set a new PIN
8. If the wrong PIN is entered 3 times, the safe can only be unlocked using the manager PIN
9. Enter the manager PIN to unlock the safe and erase the user PIN

## Safe Indicators
- **Red LED:** Safe is locked
- **Green LED:** Safe is unlocked
- **Buzzer:** Provides audio feedback
- **OLED Display:** Shows the safe status and whether a user PIN is set

## PIN Information
- **User PIN:** A 6-digit PIN created by the user
- **Manager PIN:** 272665
- The user PIN and safe state are saved so they remain stored after the Pico restarts

## Components Used
- Raspberry Pi Pico 2 W
- 3x4 Matrix Keypad
- Red LED
- Green LED
- Current-limiting resistors
- Buzzer
- 128x32 SSD1306 OLED Display
- Breadboard and jumper wires

## Pin Connections

| Component | Pico GPIO Pin |
|---|---|
| Keypad Column 1 | GP3 |
| Keypad Column 2 | GP4 |
| Keypad Column 3 | GP5 |
| Keypad Row 1 | GP6 |
| Keypad Row 2 | GP7 |
| Keypad Row 3 | GP8 |
| Keypad Row 4 | GP9 |
| Red LED | GP10 |
| Green LED | GP11 |
| Buzzer | GP13 |
| OLED CS | GP20 |
| OLED RST | GP17 |
| OLED D/C | GP16 |
| OLED CLK | GP18 |
| OLED DATA | GP19 |

## Files
- `safe_project.c` - The main C program for the safe
- `CMakeLists.txt` - Project build configuration
- `cs390_lfs/` - LittleFS storage library files
- `Hardware Setup.pdf` - Hardware setup instructions
- `User Information.pdf` - How-to Guide

## What I Learned
- How to program the Raspberry Pi Pico 2 W using C
- How to use GPIO pins with a keypad, LEDs, and buzzer
- How to communicate with an OLED display using SPI
- How to use both cores on the Raspberry Pi Pico 2 W
- How to save data using LittleFS
- How to store a PIN so it remains saved after restarting the Pico
- How to create a keypad-controlled locking system

## How to Run
1. Connect the Raspberry Pi Pico 2 W to your computer
2. Open the project in Visual Studio Code
3. Make sure the Raspberry Pi Pico SDK and required LittleFS files are included
4. Build the project using the Raspberry Pi Pico extension
5. Connect the Pico to your computer and upload the program
6. Use the keypad to set a PIN and operate the safe
