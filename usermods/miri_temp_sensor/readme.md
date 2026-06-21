# Miri Temp Sensor Usermod

Standalone Miri board temperature usermod for TMP1075 on I2C.

## Build

- Enable flag: `-D USERMOD_MIRI_TEMP_SENSOR`
- Add usermod folder in env: `custom_usermods = miri_temp_sensor`
- Example env: `esp32dev_miri_temp_sensor` in `platformio.ini`

## Runtime behavior

- I2C sensor: TMP1075 at `0x49`
- Reads temperature register `0x00` as signed 12-bit value (`0.0625 C/LSB`)
- Poll interval config: `pollIntervalMs` (default `2000`)
- Over-temp threshold config: `overTempThreshold_C` (default `70.0`)
- Hysteresis: clears over-temp at `threshold - 2C`

## Integration points

- Writes `MiriState.tempCelsius` and `MiriState.overTemp`
- Adds `Board Temp` and `Temp Alert` entries in `/json/info` (`addToJsonInfo`)
- Registers route `GET /miri/panel/tempsensor`

## Standalone fallback shims

If Miri core headers are absent:

- `MIRI_I2C_SDA` defaults to `21`
- `MIRI_I2C_SCL` defaults to `22`
- a local `MiriState` shim provides `tempCelsius` and `overTemp`

All serial logs are gated by `MIRI_DEBUG`.
