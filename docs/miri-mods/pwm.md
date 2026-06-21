# MiriPWM Mod Specification

**Branch:** `mod/pwm`  
**Build flag:** `-D USERMOD_MIRI_PWM`  
**Standalone:** Yes  
**Full spec:** [source/MiriPWM_Design_Spec.docx](source/MiriPWM_Design_Spec.docx)

## Purpose

Control Miri v1.0 LED outputs: four RGBW PWM channels (MOSFET gates), two addressable strip channels (via SN74HCS244), CH_EN enable line, and PCA9633 I²C indicator driver at 0x62.

## Hardware summary

| Channel | GPIO | Notes |
|---------|------|-------|
| LED_R | IO13 | Shared with Strip1 Data — mutually exclusive |
| LED_G | IO12 | Strapping pin; shared with Strip2 Clock |
| LED_B | IO14 | Shared with Strip2 Data |
| LED_W | IO27 | |
| Strip1 Clock | IO15 | Strapping pin |
| CH_EN | TBD | Assert HIGH to enable U5 (SN74HCS244 OE#) |
| I²C | IO21/22 | PCA9633 at 0x62 |

Default LEDC: 1220 Hz, 12-bit. Check WLED-reserved LEDC channels before assigning.

## Reference code (do not branch from)

- `Miri-Pwm-CHhip-Dev`: `usermods/usermod_v2_pca9632_Bus/`, `wled00/bus_manager.cpp` changes — port patterns only, rewrite on `main`

## Key deliverables

- Wire.begin + PCA9633 init (clear SLEEP)
- Per-channel mode config: PWM vs strip (mutual exclusion on shared GPIOs)
- CH_EN asserted HIGH on normal boot
- WLED bus integration for strip channels when strip mode active
- V1: no drawer. Reserve v2 config keys (`ch1ColorRole`, etc.) unused

## Acceptance criteria

- [ ] Compiles standalone with `USERMOD_MIRI_PWM`
- [ ] RGBW LEDC outputs update on WLED state change
- [ ] Shared GPIO mutual exclusion enforced in config
- [ ] PCA9633 I²C writes only on change
- [ ] loop() stays under 1ms typical
- [ ] Serial gated behind `MIRI_DEBUG`
