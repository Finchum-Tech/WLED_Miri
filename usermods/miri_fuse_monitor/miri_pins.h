#pragma once

// Default fuse monitor ADC pin.
// NOTE: GPIO0 is currently the tentative hardware target but is not finalized.
// On classic ESP32, GPIO0 is ADC2 and conflicts with Wi-Fi ADC access.
// Override with -DMIRI_PIN_ADC_FUSE=<gpio> if hardware changes.
#ifndef MIRI_PIN_ADC_FUSE
#define MIRI_PIN_ADC_FUSE 0
#endif
