# Miri WLED Mod Specifications

Each mod is implemented on its own branch from `main` using the naming pattern `mod/<short-name>`.

**Branch policy**
- Base branch: `main`
- Do **not** branch from `Miri-Pwm-CHhip-Dev`
- `Miri-Pwm-CHhip-Dev` may be referenced for in-progress patterns (e.g. PCA9632 bus code) but is not the integration base

**Full design documents** (Word): [`source/`](source/)

| Mod | Branch | Build flag | Spec |
|-----|--------|------------|------|
| MiriCore | `mod/miri-core` | `USERMOD_MIRI` | [miri-core.md](miri-core.md) |
| MiriTempSensor | `mod/temp-sensor` | `USERMOD_MIRI_TEMP_SENSOR` | [temp-sensor.md](temp-sensor.md) |
| MiriFuseMonitor | `mod/fuse-monitor` | `USERMOD_MIRI_FUSE_MONITOR` | [fuse-monitor.md](fuse-monitor.md) |
| MiriPWM | `mod/pwm` | `USERMOD_MIRI_PWM` | [pwm.md](pwm.md) |
| IICIe | `mod/iicie` | `USERMOD_MIRI_BOARD_IICIE` | [iicie.md](iicie.md) |
| MiriColorSphere | `mod/color-sphere` | `USERMOD_MIRI_COLOR_SPHERE` | [color-sphere.md](color-sphere.md) |

**Shared framework** (created by MiriCore worker, consumed by others): `usermods/miri/` — see MiriCore spec.

**Developer instruction (all mods):** Preserve base WLED code. Ignore Miri-specific code in `Miri_v1.0` branch (especially `usermods/usermod_v2_pca9632`). Write from scratch per spec; use existing code as reference only.
