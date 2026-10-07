# MiriCore (`usermods/miri`)

MiriCore is the integration layer for Miri usermods. It provides shared state/pin headers, branding routes, and the Miri build bundle environment.

## Included files

- `miri_core.cpp` - `USERMOD_MIRI` usermod scaffold
- `miri_shared.h` - `MiriState` shared runtime fields
- `miri_pins.h` - pin defaults with `#ifndef` guards
- `miri_env.ini` - `env:miri_esp32` build bundle
- `data/` - web overlay assets for Miri panel loader/branding

## Routes provided by MiriCore

- `GET /miri-ui.js`
- `GET /miri-brand.css`
- `GET /miri/panel/core`

Sub-mods should provide their own panel fragments under `GET /miri/panel/<modname>`.

## UI integration contract

`miri-ui.js` looks for `#miri-panels` and requests:

- `/miri/panel/tempsensor`
- `/miri/panel/fusemonitor`
- `/miri/panel/pwm`
- `/miri/panel/iicie`
- `/miri/panel/colorsphere`

Missing routes are treated as optional and ignored by the loader.

## Build usage

`platformio.ini` includes `usermods/miri/miri_env.ini`, which defines:

- `env:miri_esp32`
- `-D USERMOD_MIRI`
- bundled sub-mod flags (`TEMP_SENSOR`, `FUSE_MONITOR`, `PWM`, `BOARD_IICIE`, `COLOR_SPHERE`)

## Debug logging

All MiriCore serial logging is gated by `MIRI_DEBUG`.

