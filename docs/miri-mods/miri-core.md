# MiriCore Mod Specification

**Branch:** `mod/miri-core`  
**Build flag:** `-D USERMOD_MIRI`  
**Standalone:** No — integration layer for all Miri sub-mods  
**Full spec:** [source/MiriCore_Design_Spec.docx](source/MiriCore_Design_Spec.docx)

## Purpose

Integration layer: sub-mod registration, Miri branding, alert management, I²C bus broker (pin definitions only — not bus owner), `miri-ui.js` panel loader.

## Key deliverables

- `usermods/miri/miri_env.ini` — PlatformIO bundle env `miri_esp32` with all Miri build flags
- `usermods/miri/miri_pins.h` — abstract GPIO/I²C pin names with `#ifndef` guards
- `usermods/miri/miri_shared.h` — `MiriState` struct shared by sub-mods
- MiriCore usermod: branding, alert loop, sub-mod instantiation order, HTTP routes for `/miri-ui.js` and `/miri-brand.css`
- `usermods/miri/data/` — forked WLED web assets with `#miri-panels` div and miri-ui.js script tag

## Sub-mod registration order

1. MiriTempSensor  
2. MiriFuseMonitor  
3. MiriPWM  
4. MiriDaughterboards (IICIe)  
5. MiriColorSphere  

## Build bundling

When `USERMOD_MIRI` is enabled via `miri_esp32` env, bundle all sub-mod flags in `miri_env.ini`. Individual sub-mods remain usable standalone with their own `-D` flags.

## Reference only

- `Miri-Pwm-CHhip-Dev` branch for patterns — do not merge or branch from it
- `Miri_v1.0` / `usermod_v2_pca9632` — ignore, rewrite per spec

## Acceptance criteria

- [ ] Compiles with `-e miri_esp32` (or equivalent env name in `miri_env.ini`)
- [ ] `serverDescription` set to `"Miri"`; teal accent `#2EB8B8` in web assets
- [ ] `miri-ui.js` fetches `/miri/panel/[modname]` fragments into `#miri-panels`
- [ ] Alert loop reads `MiriState.overTemp` and `MiriState.fuseBlown` with configurable interval
- [ ] All Serial output gated behind `#ifdef MIRI_DEBUG`
- [ ] No direct hardware ownership in MiriCore (broker/branding only)
