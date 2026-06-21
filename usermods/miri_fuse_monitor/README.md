# Miri Fuse Monitor (Standalone Usermod)

Build flag:

`-D USERMOD_MIRI_FUSE_MONITOR`

## Features

- ESP32 ADC VIN sampling with `ADC_ATTEN_DB_2_5`
- ADC characterization via `esp_adc_cal_characterize()`
- 8-sample moving average
- Divider scale conversion `VIN_mV = VADC_mV * 29.67`
- Debounced fuse blow detection using:
  - consecutive low reads (`fuseDebounceReads`)
  - minimum low duration (`fuseBlowDebounceMs`)
- Updates:
  - `MiriState.voltageIn_mV`
  - `MiriState.fuseBlown`
- Info JSON fields:
  - `Vin`
  - `Fuse`
- Panel route:
  - `/miri/panel/fusemonitor`

## Config Keys

Stored under `MiriFuseMonitor` in `cfg.json`:

- `pollIntervalMs` (default `500`)
- `fuseBlowThreshold_mV` (default `1000`)
- `underVoltageThreshold_mV` (default `10500`)
- `overVoltageThreshold_mV` (default `14500`)
- `fuseDebounceReads` (default `3`)
- `fuseBlowDebounceMs` (default `1500`)
- `pin[0]` (default `MIRI_PIN_ADC_FUSE`)

## Pin default and GPIO0 caution

Default pin is defined in `miri_pins.h`:

```c
#ifndef MIRI_PIN_ADC_FUSE
#define MIRI_PIN_ADC_FUSE 0
#endif
```

GPIO0 is intentionally documented as tentative. On classic ESP32, GPIO0 is an
ADC2 pin and a boot strapping pin, so ADC reads are gated while Wi-Fi is active.
Override with build flags when hardware pinout is finalized:

`-D MIRI_PIN_ADC_FUSE=<gpio>`
