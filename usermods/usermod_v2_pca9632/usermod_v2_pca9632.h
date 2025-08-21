#pragma once
#include "wled.h"
#include <Wire.h>

/*
 * Usermod V2: PCA9632 (4-ch I2C LED driver) — cosine flare demo
 * - I2C: SDA=9, SCL=10, addr 0x62 (8-pin PCA9632)
 * - EN on GPIO5 (LOW = enable external buffer/driver)
 * - Outputs: Totem-pole + INVERTED (MODE2=0x14) for low-side MOSFETs
 * - Group dimming/blink registers are NOT written (we ignore GRP features)
 * - Startup & periodic flare on CH0 using cosine/ease curve: 1 → 100 → 0 over 5 s
 * - Flare repeats every FLARE_INTERVAL_MS
 * - I2C writes are rate-limited to 100 tx/sec and only-on-change
 * - GPIO2 heartbeat at 1 Hz for usermod liveness
 */

#ifndef USERMOD_ID_PCA9632
#define USERMOD_ID_PCA9632 0x13A4
#endif

class UsermodPCA9632 : public Usermod {
public:
  // ---- pins & address ----
  static constexpr uint8_t I2C_ADDR = 0x62;
  static constexpr int     PIN_SDA   = 9;
  static constexpr int     PIN_SCL   = 10;
  static constexpr int     PIN_EN    = 5;   // LOW = enable
  static constexpr int     PIN_BLINK = 2;   // heartbeat LED

  // ---- PCA9632 registers ----
  static constexpr uint8_t REG_MODE1   = 0x00;
  static constexpr uint8_t REG_MODE2   = 0x01;
  static constexpr uint8_t REG_PWM0    = 0x02; // PWM0..PWM3 = 0x02..0x05
  static constexpr uint8_t REG_LEDOUT  = 0x08;
  // NOTE: We intentionally do NOT touch REG_GRPPWM (0x06) or REG_GRPFREQ (0x07)

  // ---- flare timing/shape ----
  static constexpr uint32_t FLARE_TOTAL_MS = 5000;        // 5 s total
  static constexpr uint32_t FLARE_HALF_MS  = FLARE_TOTAL_MS / 2; // 2.5 s up/down
  static constexpr uint8_t  FLARE_MIN      = 1;           // start at 1 (not 0)
  static constexpr uint8_t  FLARE_PEAK     = 100;         // peak at 100

  // Trigger a new flare every X ms 
  static constexpr uint32_t FLARE_INTERVAL_MS = 7000;

  // ---- I2C rate limiting ----
  static constexpr uint32_t I2C_MIN_INTERVAL_MS = 10;     // 100 tx/sec

private:
  // runtime state
  bool     _i2cOk = false;

  // heartbeat
  bool     _blink = false;
  uint32_t _blinkTs = 0;

  // flare schedule/state
  bool     _flareActive = false;
  uint32_t _flareStart  = 0;
  uint32_t _lastFlareTrigger = 0;

  // write throttle
  uint8_t  _lastPwm0 = 255;
  uint32_t _lastI2cTs = 0;

  // --- helpers ---
  inline void i2cWrite(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
  }

  inline bool i2cBudget(uint32_t now) {
    if (now - _lastI2cTs < I2C_MIN_INTERVAL_MS) return false;
    _lastI2cTs = now;
    return true;
  }

  inline void setPWM0_ifNeeded(uint8_t v, uint32_t now) {
    if (v == _lastPwm0) return;
    if (!i2cBudget(now)) return;
    i2cWrite(REG_PWM0, v);
    _lastPwm0 = v;
  }

  void chipInit() {
    // keep buffer disabled while configuring
    pinMode(PIN_EN, OUTPUT);
    digitalWrite(PIN_EN, HIGH); // HIGH = disable

    // MODE1: wake (SLEEP=0). We leave all call/subaddress bits at reset defaults.
    i2cWrite(REG_MODE1, 0x00);

    // MODE2: DMBLNK=0 (group dim path unused), INVRT=1, OUTDRV=1, OCH=0, OUTNE=00 => 0x14
    i2cWrite(REG_MODE2, 0x14);

    // LEDOUT: all 4 channels under individual PWM control -> 0xAA
    i2cWrite(REG_LEDOUT, 0xAA);

    // clear PWM channels
    for (uint8_t ch = 0; ch < 4; ch++) i2cWrite(REG_PWM0 + ch, 0x00);

    // enable external buffer/driver
    digitalWrite(PIN_EN, LOW); // LOW = enable
  }

  // Cosine ease up/down:
  // up   (u in 0..1): v = MIN + (PEAK-MIN) * 0.5 * (1 - cos(pi*u))
  // down (u in 0..1): v =       PEAK      * 0.5 * (1 + cos(pi*u))
  bool runFlare(uint32_t now) {
    const uint32_t t = now - _flareStart;
    if (t >= FLARE_TOTAL_MS) {
      setPWM0_ifNeeded(0, now);
      return false; // finished
    }

    uint8_t v;
    if (t < FLARE_HALF_MS) {
      // up phase
      float u = t / (float)FLARE_HALF_MS;              // 0..1
      float eased = 0.5f * (1.0f - cosf(3.14159265f * u));
      float out = FLARE_MIN + (FLARE_PEAK - FLARE_MIN) * eased;
      v = (uint8_t)lroundf(out);
    } else {
      // down phase
      float u = (t - FLARE_HALF_MS) / (float)FLARE_HALF_MS;  // 0..1
      float eased = 0.5f * (1.0f + cosf(3.14159265f * u));
      float out = FLARE_PEAK * eased;                 // returns to ~0
      v = (uint8_t)lroundf(out);
    }

    setPWM0_ifNeeded(v, now);
    return true;
  }

public:
  // ---- V2 API ----
  uint16_t getId() override { return USERMOD_ID_PCA9632; }

  void setup() override {
    // heartbeat LED
    pinMode(PIN_BLINK, OUTPUT);
    digitalWrite(PIN_BLINK, LOW);
    _blinkTs = millis();

    // I2C
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(400000);

    // probe device
    Wire.beginTransmission(I2C_ADDR);
    _i2cOk = (Wire.endTransmission() == 0);

    if (_i2cOk) {
      chipInit();
      // start first flare immediately
      _flareActive      = true;
      _flareStart       = millis();
      _lastFlareTrigger = _flareStart;
      _lastPwm0         = 255;   // force first write
      _lastI2cTs        = 0;
    }
  }

  void loop() override {
    const uint32_t now = millis();

    // 1) heartbeat (always)
    if (now - _blinkTs >= 500) {
      _blink = !_blink;
      digitalWrite(PIN_BLINK, _blink ? HIGH : LOW); // flip if your LED is active-low
      _blinkTs = now;
    }

    if (!_i2cOk) return;

    // 2) trigger flare periodically (every FLARE_INTERVAL_MS) when idle
    if (!_flareActive && (now - _lastFlareTrigger >= FLARE_INTERVAL_MS)) {
      _flareActive      = true;
      _flareStart       = now;
      _lastFlareTrigger = now;
    }

    // 3) run active flare
    if (_flareActive) {
      if (!runFlare(now)) {
        _flareActive = false; // finished; waits until next interval to restart
      }
    }
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject u = root["u"];
    if (u.isNull()) u = root.createNestedObject("u");
    JsonArray arr = u.createNestedArray(F("PCA9632"));
    arr.add(_i2cOk ? F("OK") : F("I2C not found"));
    arr.add(F("GPIO2 blink 1Hz"));
    arr.add(F("Cosine flare CH0: 1→100→0 over 5s"));
    arr.add(F("Repeats every 5s"));
    arr.add(F("I2C 0x62 SDA=9 SCL=10; EN=5 (LOW=on)"));
    arr.add(F("MODE2: Totem+Invert; GRP regs untouched"));
  }

  bool readFromConfig(JsonObject&) override { return true; }
  void addToConfig(JsonObject&) override {}
};
