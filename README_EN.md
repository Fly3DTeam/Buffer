# Fly Buffer Firmware

This is the PlatformIO firmware project for the Fly Buffer/LLL Buffer module. It monitors filament buffer position, drives a BMG feeder, reports runout and blockage states, and communicates with a TMC2225 stepper driver.

Chinese documentation is available in [README.md](README.md). Version history is maintained in [CHANGELOG.md](CHANGELOG.md).

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
| Filament LED | `START_LED / PA8` | Filament/mode indication |

## Behavior

- In automatic control, the stop position has the highest priority. When detected, it disables STEP pulses immediately before the main loop completes current/mode/enable recovery; a single-button long-press manual command has priority over the stop position.
- Normal speed is set with the `speed` command in `mm/s`. Default: `30mm/s`.
- Acceleration is set with the `accel` command in `mm/s^2`. Default: `500mm/s^2`.
- Direction is controlled by buffer sensors, buttons, or external forward/back signals.
- The firmware provides TPU and non-TPU filament modes. The selected mode is stored in EEPROM and survives power cycles; new devices and first upgrades from older firmware default to non-TPU mode.
- TPU mode uses a fixed `400mA RMS` run current and disables boost. After stopping, the driver remains enabled with `IHOLD=0`, the minimum nonzero hold current (`1/32` of full scale, about `30mA RMS` with the default configuration), and is disabled after `30s` of inactivity. Firmware explicitly sets `PWMCONF.freewheel=0`, so this remains normal current regulation rather than freewheeling or passive braking.
- Non-TPU mode permits boost to `100mm/s` after more than `100mm` of movement without a position change. The driver is disabled immediately whenever the motor stops, with no standstill current.
- Boost raises current and selects high-speed mode. On exit it ramps back to normal speed using `accel`, then restores normal current and silent mode.
- Boost is disabled when there is no filament.
- The firmware uses 16 microsteps everywhere to reduce STEP interrupt load and improve stability at `100mm/s`.

## Button Controls

- Single-click either button to clear the pause/error state and resume automatic operation.
- Double-click either button to pause automatic operation and stop the motor.
- Hold the back button for manual retraction, or hold the forward button for manual feeding. A single-button long press has the highest priority and ignores all three buffer sensors, including the HALL2 stop position, until the button is released. External forward/back signals do not bypass the sensors.
- Press both buttons together to switch between TPU and non-TPU modes and save the selection to EEPROM. Both buttons must be released before another switch is accepted.
- Switching TPU/non-TPU mode does not trigger an extra status-LED indication; the status LED continues showing the current operating or fault state.

## Filament Runout

- If the device powers on with no filament, runout is reported immediately.
- The 10-second delay is only used after filament was present and then becomes absent.
- During the 10-second delay, the buffer continues working; only the runout output and indication are delayed.
- If filament is reloaded during the delay, the delay is cancelled and normal operation resumes.
- If filament is removed again during the delay, the timer starts again.

## Timeout Recovery

- If forward feeding exceeds the configured `timeout`, the firmware enters timeout error and stops.
- A single click, manual button control, a dual-button filament-mode switch, or any filament-state change (reload/removal) clears a timeout error and resumes operation.
- The same recovery actions also clear pause mode; double-clicking either individual button still explicitly enters pause mode.
- Other manually triggered errors are not cleared as timeout errors.

## Status LED

All `ERR_LED / PA15` indications are listed below:

| Indication | Timing | Repeats | Meaning |
| --- | --- | --- | --- |
| Triple fast flash and pause | Each on/off interval is `80ms`, followed by `700ms` off | Yes | Repeated TMC communication, configuration, or driver-status fault |
| Continuous fast flash | Toggles every `50ms` | Yes | Filament blockage detected |
| Continuous ultra-fast flash | Toggles every `25ms` | Yes | Forward motion exceeded `timeout` |
| Solid on | Continuously on | Yes | Paused by double-clicking a button |
| Double flash | `100ms` on, `100ms` off, `100ms` on, then `600ms` off | Yes | Normal operation with an MDM module connected |
| Slow blink | `500ms` on and `500ms` off | Yes | Normal operation without an MDM module |

When multiple states are active, the display priority is: TMC fault → blockage → forward timeout → pause → MDM connected → normal without MDM.

`START_LED / PA8` is the filament LED. It blinks with filament present in TPU mode, stays solid on with filament present in non-TPU mode, and turns off after runout is confirmed.

### Identifying TPU and Non-TPU Modes

- TPU mode: the filament LED blinks while filament is present.
- Non-TPU mode: the filament LED stays solid on while filament is present.

## Default Parameters

All configurable parameters below are stored in EEPROM and survive power cycles:

| Parameter | Default | Unit/meaning | How to change |
| --- | ---: | --- | --- |
| Filament mode | Non-TPU | Enables boost and disables the driver immediately after stopping | Press both buttons |
| Forward timeout `timeout` | `60000` | `ms`; enters timeout error after `60s` of continuous forward motion | `timeout <value>` |
| Normal speed `speed` | `30` | `mm/s` | `speed <value>` |
| Acceleration/deceleration `accel` | `500` | `mm/s²` | `accel <value>` |
| Non-TPU run current `I_CURRENT` | `500` | `mA RMS`; command range `100–3000mA` | `I <value>` |
| Host extrusion conversion `steps` | `916` | `pulse/mm`; range `1–51200` | `steps <value>` |
| MDM encoder length `encoder_length` | `1.73` | `mm/pulse` | `encoder <value>` |
| Blockage error scale `allow_error_scale` | `2` | Default allowed error: `1.73 × 2 = 3.46mm` | `scale <value>` |
| Runout active level | `0` | `DUANLIAO` active low | `out <0/1>` |

The main fixed parameters are:

| Parameter | Default | Meaning |
| --- | ---: | --- |
| Runout confirmation delay | `10s` | After filament has previously been present, runout is confirmed after this continuous no-filament interval |
| Boost trigger distance | `100mm` | Available only in non-TPU mode |
| Boost target speed | `100mm/s` | Used when normal speed is lower |
| Boost current multiplier | `1.2` | `120%` of run current, capped in code at `3000mA` |
| TPU run current | `400mA RMS` | Fixed in TPU mode and unaffected by the `I` command |
| TPU standstill current | `IHOLD=0` | `1/32` of full scale, about `30mA RMS` with default configuration |
| TPU stopped disable delay | `30s` | Standstill holding current remains active during this interval |
| Non-TPU stopped disable delay | `0s` | Driver is disabled immediately after stopping |
| Motor microsteps | `16` | Shared by normal and boosted motion |
| BMG gear ratio | `3:1` | Used for motor STEP conversion |
| BMG drive circumference | `22.93mm` | Used for motor STEP conversion |
| Watchdog timeout | About `4s` | A stalled main loop normally resets in about `4–5s` |

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
| `I <value>` | Set non-TPU run current, range `100-3000mA` |
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
