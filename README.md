# FLY-LLL PLUS Buffer Firmware

This is the firmware source code for the FLY-LLL PLUS Buffer, a filament buffer system for 3D printers. The buffer automatically manages filament tension using three optical sensors and a motor-driven feeding mechanism.

# What This Firmware Does

The buffer monitors filament position using three optical sensors (HALL1, HALL2, HALL3) and automatically:
- **Feeds filament forward** when tension is low (sensor 1 blocked)
- **Stops feeding** when tension is balanced (sensor 2 triggered)
- **Retracts filament** when tension is too high (sensor 3 triggered)
- **Detects filament runout** and signals your 3D printer
- **Supports MDM blockage detection module** (optional, v1.1.x feature)

---

# Installation & Setup

### Prerequisites

1. **Install VS Code** (if not already installed)
   - Download from: https://code.visualstudio.com/

2. **Install PlatformIO IDE Extension**
   - Open VS Code
   - Click the Extensions icon on the left sidebar (or press `Ctrl+Shift+X` / `Cmd+Shift+X`)
   - Search for "PlatformIO IDE"
   - Click "Install"

3. **Open This Project**
   - In VS Code: File → Open Folder → Select this Buffer folder
   - PlatformIO will automatically install dependencies

4. **Install dfu-util** (Optional - only needed for command-line flashing method)
   - **macOS**: Install via Homebrew: `brew install dfu-util`
   - **Linux**: Install via package manager:
     - Ubuntu/Debian: `sudo apt-get install dfu-util`
     - Fedora: `sudo dnf install dfu-util`
     - Arch: `sudo pacman -S dfu-util`
   - **Windows**: Download from: https://dfu-util.sourceforge.net/releases/ or use the version included with PlatformIO (usually in `.platformio/packages/tool-dfuutil/bin/`)

### Compiling the Firmware

1. Click the PlatformIO icon in the left sidebar (looks like an ant head)
2. In the PlatformIO toolbar at the bottom, click "Build" (checkmark icon)
3. Wait for compilation to complete
4. The compiled `.bin` file will be in `.pio/build/stm32f072c8/firmware.bin`

### Flashing the Firmware

**Method 1: Using PlatformIO (Recommended)**

1. Put your STM32 board into DFU (Device Firmware Update) mode:
   - Hold the **BOOT0** button
   - Press and release the **RESET** button
   - Release **BOOT0**
   - The board should now be in DFU mode

2. In PlatformIO, click "Upload" (arrow icon)
3. Wait for the upload to complete
4. Press **RESET** to exit DFU mode

**Method 2: Using dfu-util (Command Line)**

1. Put board in DFU mode (same as Method 1, step 1)
2. Open terminal in the project folder
3. Run: `dfu-util -a 0 -s 0x08000000:leave -D .pio/build/stm32f072c8/firmware.bin`
4. Press **RESET** to exit DFU mode

---




# Configuration Guide

There are two ways to configure the buffer:

## Option A: Runtime Configuration (Easy - No Recompiling Required)

You can change settings while the buffer is running using serial commands. These settings are saved to EEPROM and persist after power cycles.

#### Step 1: Connect to Serial Monitor

1. Connect the buffer to your computer via USB
2. In VS Code, open PlatformIO
3. Click "Serial Monitor" (plug icon) in the PlatformIO toolbar
4. Set baud rate to **115200**
5. Make sure "Newline" is set to "Both NL & CR" or "LF"

#### Step 2: Available Serial Commands

**View Current Speed**
```
speed
```
*Example output: `speed: 260`*

**Change Motor Speed**
```
speed 500
```
- Sets motor speed to 500 RPM
- Default: 260 RPM
- Range: Any positive number (typical: 200-800 RPM)
- **Example**: To load filament faster, use `speed 500`. To slow down for delicate filaments, use `speed 200`

**View Current Timeout**
```
rt
```
*Example output: `read timeout=60000`*

**Change Timeout**
```
timeout 90000
```
- Sets timeout to 90 seconds (90000 milliseconds)
- Default: 60000 ms (60 seconds)
- Range: Any positive number (typical: 30000-120000 ms)
- **What it does**: If the motor runs forward continuously for this duration without stopping, an error is triggered
- **Example**: If your filament loading takes longer, increase timeout: `timeout 90000`

**View All Settings**
```
info
```
*Example output:*
```
encoder_length=100.00
timeout=60000
steps=100
allow_error_scale=0.05
allow_error=5.00
```

**MDM Module Settings** (Only if using MDM blockage detection module)

```
steps 100
```
- Sets steps per millimeter for MDM module
- Range: 0-51200
- **Example**: If your extruder uses 100 steps/mm, set: `steps 100`

```
encoder 100.5
```
- Sets encoder length in millimeters
- Range: Any positive number
- **Example**: If your encoder measures 100.5mm, set: `encoder 100.5`

```
scale 0.05
```
- Sets error tolerance scale (as a decimal)
- Range: Any positive number (typical: 0.01-0.10)
- **Example**: For 5% tolerance, use: `scale 0.05`

**Get Help**
```
help
```
- Displays all available commands

#### Step 3: Verify Settings Persist

1. Change a setting (e.g., `speed 500`)
2. Power cycle the buffer (unplug and replug USB)
3. Check the setting again (`speed`)
4. It should still show your custom value (500)

---

## Option B: Compile-Time Configuration (Advanced - Requires Recompiling)

For settings that can't be changed via serial, you need to modify the source code and recompile.

#### Step 1: Open Configuration Files

The main settings are in: `lib/buffer/buffer.h`

#### Step 2: Adjust Motor Current

**How to Find:**
1. Open `lib/buffer/buffer.h`
2. Press `Ctrl+F` (Windows/Linux) or `Cmd+F` (Mac) to open Find
3. Search for: `I_CURRENT`
4. You should find this line:
   ```cpp
   #define I_CURRENT  (500) // Motor current in mA / 电流(mA)
   ```

**How to Change:**
1. Change `500` to your desired current (in milliamps)
4. **Example**: For a 0.5A motor, use `500`. For a 1A motor, use `1000`
5. Save the file
6. Recompile and flash (see Installation section above)

**Important Notes:**
- Higher current = more torque but more heat
- Check your motor specifications before increasing
- Typical range: 300-1000 mA
- **Example**: If motor stalls, try increasing to `600` or `700`

#### Step 3: Adjust Microsteps

**How to Find:**
1. Open `lib/buffer/buffer.h`
2. Press `Ctrl+F` (Windows/Linux) or `Cmd+F` (Mac) to open Find
3. Search for: `Move_Divide_NUM`
4. You should find this line:
   ```cpp
   #define Move_Divide_NUM ((int32_t)(64))  // Microstep subdivision
   ```

**How to Change:**
1. Change `64` to your desired microstep value
4. Common values: `16`, `32`, `64`
5. Save, recompile, and flash

**What Microsteps Do:**
- Higher values (64) = smoother motion, less torque
- Lower values (16) = more torque, less smooth
- Default (64) works well for most cases
- **Example**: If motor lacks torque, try `32` or `16`

#### Step 4: Adjust Default Timeout

**How to Find:**
1. Open `lib/buffer/buffer.cpp`
2. Press `Ctrl+F` (Windows/Linux) or `Cmd+F` (Mac) to open Find
3. Search for: `DEFAULT_TIMEOUT`
4. You should find this line:
   ```cpp
   const uint32_t DEFAULT_TIMEOUT = 60000;  // Default timeout: 60 seconds
   ```

**How to Change:**
1. Change `60000` to your desired default (in milliseconds)
4. **Example**: For 90 seconds default, use `90000`
5. Save, recompile, and flash

**Note**: You can still override this via serial command (`timeout`), but this sets the initial value.

#### Step 5: Adjust Default Speed

**How to Find:**
1. Open `lib/buffer/buffer.h`
2. Press `Ctrl+F` (Windows/Linux) or `Cmd+F` (Mac) to open Find
3. Search for: `static int32_t SPEED`
4. You should find this line:
   ```cpp
   static int32_t SPEED = 260;     // Motor speed in RPM
   ```

**How to Change:**
1. Change `260` to your desired default RPM
4. **Example**: For faster default loading, use `400`
5. Save, recompile, and flash

**Note**: You can still override this via serial command (`speed`), but this sets the initial value.

---

# Complete Command Reference

| Command | Description | Example | Default |
|---------|-------------|---------|---------|
| `speed` | View current motor speed (RPM) | `speed` | 260 |
| `speed <n>` | Set motor speed (RPM) | `speed 500` | 260 |
| `rt` | Read current timeout (ms) | `rt` | 60000 |
| `timeout <n>` | Set timeout (milliseconds) | `timeout 90000` | 60000 |
| `info` | Show all current settings | `info` | - |
| `steps <n>` | Set steps per mm (MDM module) | `steps 100` | - |
| `encoder <n>` | Set encoder length in mm (MDM) | `encoder 100.5` | - |
| `scale <n>` | Set error tolerance scale (MDM) | `scale 0.05` | - |
| `help` | Show command help menu | `help` | - |

---

# Common Use Cases

### Use Case 1: Faster Filament Loading

**Problem**: Filament loads too slowly

**Solution**:
1. Connect via serial monitor
2. Type: `speed 500` (or higher, like 600-800)
3. Press Enter
4. Test loading speed
5. Adjust as needed

### Use Case 2: Motor Stalling

**Problem**: Motor stops or makes noise but doesn't move

**Solutions to Try** (in order):
1. **Increase motor current** (compile-time):
   - Open `lib/buffer/buffer.h`
   - Press `Ctrl+F` (Windows/Linux) or `Cmd+F` (Mac) and search for: `I_CURRENT`
   - Change `500` to `600` or `700`
   - Recompile and flash
2. **Reduce microsteps** (compile-time):
   - Open `lib/buffer/buffer.h`
   - Press `Ctrl+F` (Windows/Linux) or `Cmd+F` (Mac) and search for: `Move_Divide_NUM`
   - Change `64` to `32` or `16`
   - Recompile and flash
3. **Reduce speed** (runtime):
   - Type: `speed 200`
   - Press Enter

### Use Case 3: Timeout Errors

**Problem**: Buffer stops with error during long filament loads

**Solution**:
1. Connect via serial monitor
2. Type: `timeout 90000` (90 seconds) or higher
3. Press Enter
4. Settings are saved automatically

### Use Case 4: Delicate Filament

**Problem**: Need gentler handling for flexible or brittle filament

**Solution**:
1. Connect via serial monitor
2. Type: `speed 200` (slower speed)
3. Press Enter
4. Test and adjust as needed

---

# Troubleshooting

### Serial Monitor Shows Nothing

**Possible Causes:**
1. Wrong baud rate → Set to **115200**
2. Wrong USB port → Check Device Manager (Windows) or `ls /dev/cu.*` (Mac/Linux)
3. Board not powered → Check USB connection
4. Board in DFU mode → Press RESET button

### Settings Don't Persist After Power Cycle

**Solution:**
- Settings are saved to EEPROM automatically
- If they don't persist, the EEPROM may be corrupted
- Try setting the value again via serial command
- If problem continues, reflash firmware

### Motor Doesn't Move

**Checklist:**
1. Is motor connected properly?
2. Is power supply adequate? (24V recommended)
3. Check motor current setting (try increasing in `buffer.h`)
4. Check microsteps setting (try reducing in `buffer.h`)
5. Check speed setting (try `speed 300` via serial)

### Compilation Errors

**Common Issues:**
1. PlatformIO not installed → Install PlatformIO IDE extension
2. Dependencies missing → PlatformIO should auto-install, but try: `pio lib install`
3. Wrong board selected → Check `platformio.ini` has `board = stm32f072c8`

### DFU Mode Not Working

**Troubleshooting Steps:**
1. Try different USB cable (data cable, not charge-only)
2. Try different USB port
3. Hold BOOT0 button longer before pressing RESET
4. Check if board appears in Device Manager (Windows) or `lsusb` (Linux)
5. Install STM32 DFU drivers if needed

---

# Technical Details

- **Microcontroller**: STM32F072C8
- **Motor Driver**: TMC2209 (UART mode)
- **Communication**: Serial (115200 baud)
- **Storage**: EEPROM for persistent settings
- **Firmware Version**: v1.1.x (features MDM support, watchdog timer)

---

# Support & Resources

- **Official Documentation**: https://mellow.klipper.cn/en/docs/ProductDoc/ExtensionBoard/fly-buffer-plus/
- **GitHub Repository**: Check for latest updates and issues

---

# Notes

- All runtime settings (speed, timeout, etc.) are saved to EEPROM automatically
- Compile-time settings require recompiling and flashing
- Default values work well for most users - only change if you have specific needs
- Motor current and microsteps affect motor performance - adjust carefully
- Higher speed = faster loading but may cause issues with some filaments
