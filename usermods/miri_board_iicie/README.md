# Miri Board IICIe Usermod

Build flag:

`-D USERMOD_MIRI_BOARD_IICIE`

This usermod provides standalone PCA9849 mux support on `Wire1` with:

- Required address registry: `0x58, 0x59, 0x5B, 0x70, 0x71, 0x73, 0x74, 0x75, 0x77`
- Channel control-byte write endpoint per detected mux
- Manual downstream scan endpoint (no boot-time downstream scan)
- Miri panel endpoint per detected mux

## HTTP routes

For each detected mux address `0x[addr]`:

- `GET /miri/panel/iicie/0x[addr]`
- `POST /miri/iicie/0x[addr]/channel` with body `channel=0..3`
- `POST /miri/iicie/0x[addr]/scan`

### Route behavior checks

- `/channel` writes one control byte to the PCA9849 control register (`B2:B1:B0 = channel`).
- `/scan` scans only the currently selected downstream channel and runs only when explicitly called.
- No downstream scan is executed during boot/setup.
- Routes are registered only for mux addresses detected on the required registry.

## Peripheral template framework

The panel includes an auto-generated drawer sourced from code declarations with copy-paste JSON snippets for:

- `AnalogPeripheral`
- `DigitalPeripheral`
- `DisplayPeripheral`

These are starter templates for downstream devices and can be expanded by adding entries to `kPeripheralTemplates`.

### Peripheral docs checks

- Each template includes a copy-paste JSON config example.
- The drawer is generated from peripheral declarations in code (`kPeripheralTemplates`).
- Template categories implemented: analog, digital, display.

## Standalone pin fallbacks

If Miri pin macros are not defined by an integration layer, the usermod falls back to:

- `MIRI_LP_I2C_SDA=21`
- `MIRI_LP_I2C_SCL=22`
- `MIRI_LP_I2C_FREQ=100000`

All serial logging is gated by `MIRI_DEBUG`.
