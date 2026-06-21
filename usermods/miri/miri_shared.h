#ifndef USERMOD_MIRI_SHARED_H
#define USERMOD_MIRI_SHARED_H

#include <Arduino.h>

struct MiriState {
  float tempCelsius = NAN;
  uint32_t voltageIn_mV = 0;
  bool overTemp = false;
  bool fuseBlown = false;
  bool alertActive = false;
  uint32_t lastAlertMs = 0;
};

extern MiriState miriState;

#endif
