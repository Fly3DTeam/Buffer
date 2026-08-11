# Fly Buffer Firmware

This is the PlatformIO firmware project for the Fly Buffer/LLL Buffer module. It monitors filament buffer position, drives a BMG feeder, reports runout and blockage states, and communicates with a TMC2225 stepper driver.

Chinese documentation is available in [README.md](README.md).

## Features

- Reads three optical buffer sensors:
  - `HALL3`: forward position, feeds filament forward.
  - `HALL2`: stop position, stops the motor immediately.
  - `HALL1`: back position, retracts filament.
- Supports the onboard filament switch and optional MDM runout/blockage module.
- Provides runout output, blockage output, status LED, and start LED.
- Drives the motor using STEP/DIR timer pulses. Speed is configured in `mm/s`.
- Uses BMG gear geometry for step-per-mm calculation.
- Uses a TMC2225 driver through the TMC2208-compatible UART register model.
- Supports serial commands for speed, acceleration, current, blockage detection, timeout, and output polarity.
- Uses a hardware watchdog to improve reliability.

## Hardware Signals

| Function | Pin | Description |
| --- | --- | --- |
| Optical sensor 1 | `HALL3 / PB4` | Forward position |
| Optical sensor 2 | `HALL2 / PB3` | Stop position |
| Optical sensor 3 | `HALL1 / PB2` | Back position |
| Filament switch | `ENDSTOP_3 / PB7` | Filament present/runout detection |
| Motor enable | `EN_PIN / PA6` | TMC enable |
| Motor direction | `DIR_PIN / PA7` | STEP/DIR direction |
| Motor step | `STEP_PIN / PC13` | Timer pulse output |
| TMC UART | `UART / PB1` | TMC2225 UART communication |
| Runout output | `DUANLIAO / PB15` | Configurable active level |
| Status LED | `ERR_LED / PA15` | Status/fault indication |
| Start LED | `START_LED / PA8` | Work-state indication |

## Behavior

- The stop position has the highest priority. When it is detected, the firmware disables STEP pulses immediately, then finishes current/mode/enable recovery in the main loop.
- Normal speed is set with the `speed` command in `mm/s`. Default: `30mm/s`.
- Acceleration is set with the `accel` command in `mm/s^2`. Default: `500mm/s^2`.
- Direction is controlled by buffer sensors, buttons, or external forward/back signals.
- If the feeder moves more than `100mm` without a position change, boost mode raises the target speed to `100mm/s`.
- Before boost, the firmware raises current and switches to high-speed mode. After stopping, it restores normal current and silent mode.
- After the motor stops, the driver remains enabled. `IRUN` keeps the configured run current while `IHOLD` is independently set to `50mA`; the TMC automatically switches to holding current at standstill.
- If no motion restarts for `30s` after stopping, the firmware disables the driver. It is enabled again immediately before the next automatic or manual move.
- Boost is disabled when there is no filament.
- The firmware uses 16 microsteps everywhere to reduce STEP interrupt load and improve stability at `100mm/s`.

## Button Controls

- Single-click either button to clear the pause/error state and resume automatic operation.
- Double-click either button to pause automatic operation and stop the motor.
- Hold the back button for manual retraction, or hold the forward button for manual feeding.
- Press both buttons together to enable or disable fast mode immediately. Both buttons must be released before it can be toggled again.
- Fast mode is enabled by default. When enabled, movement beyond `100mm` may boost to `100mm/s`. Disabling it prevents automatic boost and immediately exits an active boost.
- Enabling fast mode is confirmed by five clear flashes (`200ms` on and `200ms` off); disabling it is confirmed by a `2s` solid light. Each confirmation runs once before the normal status indication resumes. A TMC fault remains a repeating triple-fast-flash pattern.

## Filament Runout

- If the device powers on with no filament, runout is reported immediately.
- The 10-second delay is only used after filament was present and then becomes absent.
- During the 10-second delay, the buffer continues working; only the runout output and indication are delayed.
- If filament is reloaded during the delay, the delay is cancelled and normal operation resumes.
- If filament is removed again during the delay, the timer starts again.

## Timeout Recovery

- If forward feeding exceeds the configured `timeout`, the firmware enters timeout error and stops.
- A single click, manual button control, a dual-button fast-mode toggle, or any filament-state change (reload/removal) clears a timeout error and resumes operation.
- The same recovery actions also clear pause mode; double-clicking either individual button still explicitly enters pause mode.
- Other manually triggered errors are not cleared as timeout errors.

## Status LED

All `ERR_LED / PA15` indications are listed below:

| Indication | Timing | Repeats | Meaning |
| --- | --- | --- | --- |
| Five clear flashes | `200ms` on and `200ms` off, five times | No | Fast mode enabled successfully |
| Solid on, then off | On for `2s`, then off for `500ms` | No | Fast mode disabled successfully |
| Triple fast flash and pause | Each on/off interval is `80ms`, followed by `700ms` off | Yes | Repeated TMC communication, configuration, or driver-status fault |
| Continuous fast flash | Toggles every `50ms` | Yes | Filament blockage detected |
| Continuous ultra-fast flash | Toggles every `25ms` | Yes | Forward motion exceeded `timeout` |
| Solid on | Continuously on | Yes | Paused by double-clicking a button |
| Double flash | `100ms` on, `100ms` off, `100ms` on, then `600ms` off | Yes | Normal operation with an MDM module connected |
| Slow blink | `500ms` on and `500ms` off | Yes | Normal operation without an MDM module |

When multiple states are active, the display priority is: fast-mode confirmation → TMC fault → blockage → forward timeout → pause → MDM connected → normal without MDM. After a one-shot fast-mode confirmation finishes, the LED resumes the repeating indication for the current state.

`START_LED / PA8` does not use blink codes. It is on during normal operation with filament and turns off after runout is confirmed. During the `10s` runout delay, it remains in the normal working state.

## Serial Commands

Commands end with a newline.

| Command | Description |
| --- | --- |
| `info` | Show main parameters |
| `version` | Show firmware version |
| `speed` | Show speed |
| `speed <value>` | Set speed in `mm/s` |
| `accel` | Show acceleration |
| `accel <value>` | Set acceleration in `mm/s^2` |
| `I` | Show TMC current |
| `I <value>` | Set TMC current, range `100-3000mA` |
| `timeout <value>` | Set forward timeout in `ms` |
| `rt` | Show forward timeout |
| `steps <value>` | Set host pulse conversion parameter |
| `encoder <value>` | Set MDM filament length per pulse |
| `scale <value>` | Set blockage detection error scale |
| `out <0/1>` | Set runout output active level |
| `clear` | Clear blockage detection counters |
| `flyboot` | Enter bootloader |

## Build

1. Install VS Code.
2. Install the PlatformIO IDE extension.
3. Open this project directory.
4. Run PlatformIO Build, or run:

```bash
pio run
```

The active project environment is `fly_f072cb`.

## Changelog

### 2.0.1

- Updated firmware version to `2.0.1`.
- Changed motor control to STEP/DIR timer pulse output.
- Changed speed units to `mm/s`; added `accel` command for acceleration.
- Added BMG gear based step conversion.
- Unified all motion to 16 microsteps to reduce STEP interrupt load at `100mm/s`.
- Added automatic boost to `100mm/s` after more than `100mm` of movement without a position change.
- Boost mode raises current and switches to high-speed mode; stopping restores normal current and silent mode.
- Changed TMC2225 communication to use the TMC2208-compatible class.
- Added periodic TMC status/config checks with automatic reconfiguration.
- Added compensation for the TMC2225 `GCONF.en_spreadCycle` read/write inversion behavior.
- Optimized watchdog feeding and high-speed pulse generation stability.
- Made stop-position detection immediately disable STEP pulses.
- Changed runout behavior so the buffer continues working for 10 seconds before reporting runout.
- Added automatic recovery from forward-timeout error after filament reload.
- Fixed parameter safety issues such as zero `steps` and invalid current values.
