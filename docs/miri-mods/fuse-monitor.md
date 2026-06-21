# MiriFuseMonitor Mod Specification

**Branch:** `mod/fuse-monitor`  
**Build flag:** `-D USERMOD_MIRI_FUSE_MONITOR`  
**Standalone:** Yes  
**Full spec:** [source/MiriFuseMonitor_Design_Spec.docx](source/MiriFuseMonitor_Design_Spec.docx)

## Purpose

Sample input supply voltage via 43kΩ/1.5kΩ divider on ADC. Calculate VIN in mV. Detect fuse blow. Write `MiriState.voltageIn_mV` and `MiriState.fuseBlown`. Main-page drawer for live voltage/fuse status.

## Hardware

- Divider ratio: 1.5/44.5 = 0.03371 → VIN_mV = VADC_mV × 29.67
- ADC: `MIRI_PIN_ADC_FUSE` — **GPIO0 pending hardware confirmation** (boot strapping + ADC2/Wi-Fi conflict)
- Use `ADC_ATTEN_DB_2_5`, `esp_adc_cal_characterize()`, 8-sample moving average

## Critical blocker

**Do not assume GPIO0 is final.** Document GPIO0 conflict in code comments. Implement with configurable pin via `miri_pins.h` default; gate ADC2 reads appropriately if GPIO0 confirmed.

## Key deliverables

- ADC init after 500ms boot delay
- GET `/miri/panel/fusemonitor` — VIN bar, fuse OK/BLOWN badge
- Config: pollIntervalMs (500), fuseBlowThreshold_mV (1000), under/over voltage thresholds
- `addToJsonInfo()`: `"Vin"`, `"Fuse"`

## Acceptance criteria

- [ ] Compiles standalone with `USERMOD_MIRI_FUSE_MONITOR` *(tooling dependent in local environment)*
- [x] Fuse blow debounced (N consecutive reads below threshold)
- [x] MiriState fields updated each poll cycle
- [x] GPIO0 conflict documented; pin abstracted in `miri_pins.h`
- [x] Serial gated behind `MIRI_DEBUG`

## Implementation notes (current branch)

- Code path: `usermods/miri_fuse_monitor/`
- Standalone pin defaults are in `usermods/miri_fuse_monitor/miri_pins.h`
- Route implemented: `GET /miri/panel/fusemonitor`
- Info keys implemented in `addToJsonInfo()`: `Vin`, `Fuse`
- Debounce controls include both read-count and time window:
  - `fuseDebounceReads`
  - `fuseBlowDebounceMs`
