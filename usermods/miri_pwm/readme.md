# Miri PWM usermod (`USERMOD_MIRI_PWM`)

Standalone usermod for Miri v1.0 PWM/I2C control on branch `mod/pwm`.

## Scope

- RGBW analog outputs on ESP32 LEDC (`GPIO13/12/14/27`, 1220 Hz, 12-bit)
- Shared-pin strip mutual exclusion for `GPIO13/12/14`
- PCA9633 (`0x62`) init and write-on-change channel updates
- CH_EN support (asserted active on boot)
- WLED state-driven updates via `onStateChange()`
- Reserved v2 config keys persisted (unused in v1 behavior)

## Build

Use the dedicated env in `platformio.ini`:

- `esp32dev_miri_pwm`

Enabled flags:

- `-D USERMOD_MIRI_PWM`

Optional compile-time overrides:

- `-D MIRI_PWM_CH_EN_PIN=<gpio>` (default `-1`, disabled)
- `-D MIRI_PWM_CH_EN_ACTIVE_LEVEL=HIGH|LOW` (default `HIGH`)
- `-D MIRI_PWM_I2C_SDA=<gpio>` (default `21`)
- `-D MIRI_PWM_I2C_SCL=<gpio>` (default `22`)
- `-D MIRI_DEBUG` (enables debug logging)

## Config keys (`cfg.json` / Usermod settings)

Top-level usermod object: `MiriPWM`

- `ch1Mode` (R on `GPIO13`): `0` PWM, `1` strip-reserved
- `ch2Mode` (G on `GPIO12`): `0` PWM, `1` strip-reserved
- `ch3Mode` (B on `GPIO14`): `0` PWM, `1` strip-reserved
- `ch4Mode` (W on `GPIO27`): forced to PWM (strip mode ignored)
- `chEnPin`: CH_EN GPIO (`-1` disables)

Reserved for future v2 (stored but unused in v1):

- `ch1ColorRole`
- `ch2ColorRole`
- `ch3ColorRole`
- `ch4ColorRole`

## Runtime behavior notes

- Shared pins are re-evaluated periodically and on config reload.
- If a shared pin is currently used by an active digital strip bus, that PWM channel is released.
- PCA9633 writes are skipped when value has not changed.
- `loop()` does deferred work only when needed to keep runtime overhead low.
