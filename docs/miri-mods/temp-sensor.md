# MiriTempSensor Mod Specification

**Branch:** `mod/temp-sensor`  
**Build flag:** `-D USERMOD_MIRI_TEMP_SENSOR`  
**Standalone:** Yes  
**Full spec:** [source/MiriTempSensor_Design_Spec.docx](source/MiriTempSensor_Design_Spec.docx)

## Purpose

Read TI TMP1075NDRLR (U6) at I²C address `0x49` on main bus. Expose board temperature in WLED info panel and main-page drawer. Write `MiriState.tempCelsius` and `MiriState.overTemp`.

## Hardware

- I²C: SDA IO21, SCL IO22 (via `MIRI_I2C_SDA` / `MIRI_I2C_SCL` in `miri_pins.h`)
- Sensor address: 0x49 (A0 tied to VCC)
- 12-bit temp register at 0x00, 0.0625°C/LSB

## Key deliverables

- Usermod under `usermods/miri/` (or standalone path if MiriCore not present)
- Self-init I²C: `Wire.begin(MIRI_I2C_SDA, MIRI_I2C_SCL)` in setup
- GET `/miri/panel/tempsensor` drawer fragment (live gauge via `/json/info`)
- Config: `pollIntervalMs` (default 2000), `overTempThreshold_C` (default 70.0) with 2°C hysteresis

## MiriState

| Field | Action |
|-------|--------|
| `tempCelsius` | Write on successful poll |
| `overTemp` | true when temp > threshold; false when temp <= threshold - 2°C |

## Acceptance criteria

- [ ] Compiles standalone with only `USERMOD_MIRI_TEMP_SENSOR` (+ pin overrides if no MiriCore)
- [ ] Probes 0x49 at boot; graceful degradation if no ACK
- [ ] `addToJsonInfo()` emits `"Board Temp"`; alert badge when overTemp
- [ ] Drawer registered and functional
- [ ] Serial gated behind `MIRI_DEBUG`

## Implementation status (isolated worktree)

- [x] Standalone usermod added at `usermods/miri_temp_sensor/` with `library.json`
- [x] Standalone env added: `env:esp32dev_miri_temp_sensor` with `custom_usermods = miri_temp_sensor`
- [x] TMP1075 I2C read path implemented (`0x49`, register `0x00`, 12-bit signed conversion)
- [x] Polling + threshold config implemented: `pollIntervalMs`, `overTempThreshold_C`, 2C hysteresis
- [x] `MiriState.tempCelsius` and `MiriState.overTemp` writes implemented
- [x] `/json/info` includes `Board Temp` and over-temp alert entry
- [x] Route added: `GET /miri/panel/tempsensor`
- [x] Serial output gated behind `MIRI_DEBUG`
- [ ] PlatformIO build validation in this environment (blocked: `pio` command unavailable)
