# IICIe (Pixi²) Daughter Board Mod Specification

**Branch:** `mod/iicie`  
**Build flag:** `-D USERMOD_MIRI_BOARD_IICIE`  
**Standalone:** Yes  
**Full spec:** [source/IICIe_Design_Spec.docx](source/IICIe_Design_Spec.docx)

## Purpose

Driver for PCA9849PWJ 4-channel I²C multiplexer daughter board on LP bus (Wire1). Up to 9 selectable addresses via onboard switches. Extensible peripheral framework with copy-paste templates for analog/digital/display peripherals.

## Hardware

- Chip: PCA9849PWJ — channel select via 1-byte control register (bits B2:B1:B0)
- LP bus: `Wire1.begin(MIRI_LP_I2C_SDA, MIRI_LP_I2C_SCL, 100000)`
- 9 addresses: 0x58, 0x59, 0x5B, 0x70, 0x71, 0x73, 0x74, 0x75, 0x77

## Key deliverables

- `MiriBoard_IICIe` driver: activate on address detection, channel select, downstream scan
- HTTP routes per instance:
  - GET `/miri/panel/iicie/0x[addr]`
  - POST `/miri/iicie/0x[addr]/channel`
  - POST `/miri/iicie/0x[addr]/scan` (manual scan only — not at boot)
- User peripheral template section (AnalogPeripheral, DigitalPeripheral, DisplayPeripheral)
- Auto-generated drawer from peripheral declarations

## Acceptance criteria

- [ ] Compiles standalone with `USERMOD_MIRI_BOARD_IICIE`
- [ ] All 9 addresses in board registry
- [ ] Channel selection writes correct PCA9849 control byte
- [ ] Scan triggered only via UI/API, not automatic at boot
- [ ] Peripheral templates documented with copy-paste examples
- [ ] Serial gated behind `MIRI_DEBUG`
