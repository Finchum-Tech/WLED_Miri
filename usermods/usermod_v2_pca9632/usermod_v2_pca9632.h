#pragma once
#include "wled.h"
#include <Wire.h>

/*
 * Usermod V2: PCA9632 (4-ch) — MOSFET-friendly, flare 1→100→0 over 5s
 * - I2C: SDA=9, SCL=10, addr 0x62
 * - EN on GPIO5 (LOW = enable)
 * - MODE2: INVRT=1, OUTDRV=1, DMBLNK=0 (group dim)
 * - Blink GPIO2 at 1 Hz so you know it's alive
 * - On boot: one-shot flare on PWM0, 5s total, with I2C writes capped at 100 Hz
 */

#ifndef USERMOD_ID_PCA9632
#define USERMOD_ID_PCA9632 0x54
#endif

class UsermodPCA9632 : public Usermod {
public:
  // Pins & address
  static constexpr uint8_t I2C_ADDR = 0x62;
  static constexpr int PIN_SDA   = 9;
  static constexpr int PIN_SCL   = 10;
  static constexpr int PIN_EN    = 5;   // external buffer enable (LOW = on)
  static constexpr int PIN_BLINK = 2;

  // PCA9632 registers
  static constexpr uint8_t REG_MODE1   = 0x00;
  static constexpr uint8_t REG_MODE2   = 0x01;
  static constexpr uint8_t REG_PWM0    = 0x02; // ..PWM3 = 0x05
  static constexpr uint8_t REG_GRPPWM  = 0x06;
  static constexpr uint8_t REG_GRPFREQ = 0x07;
  static constexpr uint8_t REG_LEDOUT  = 0x08;

  // Flare: 5s total, 2.5s up then 2.5s down
  static constexpr uint32_t FLARE_TOTAL_MS = 5000;
  static constexpr uint32_t FLARE_HALF_MS  = FLARE_TOTAL_MS/2;
  static constexpr uint8_t  FLARE_MIN      = 0;   // 1..100..0
  static constexpr uint8_t  FLARE_PEAK     = 100;

  // I2C rate limit: 100 tx/sec
  static constexpr uint32_t I2C_MIN_INTERVAL_MS = 10;

private:
  // Runtime
  bool     _i2cOk = false;
  bool     _blink = false;
  uint32_t _blinkTs = 0;

  bool     _flareActive = false;
  uint32_t _flareStart  = 0;

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
    _lastI2cTs = now; return true;
  }
  inline void setPWM0_ifNeeded(uint8_t v, uint32_t now) {
    if (v == _lastPwm0) return;
    if (!i2cBudget(now)) return;
    i2cWrite(REG_PWM0, v);
    _lastPwm0 = v;
  }

  void chipInit() {
    // keep external buffer off while configuring
    pinMode(PIN_EN, OUTPUT);
    digitalWrite(PIN_EN, HIGH); // HIGH = disable

    // Wake, internal osc
    i2cWrite(REG_MODE1, 0x00);

    // MODE2: DMBLNK=0 (group DIM), INVRT=1, OUTDRV=1, OCH=0, OUTNE=00 -> 0x14
    i2cWrite(REG_MODE2, 0x14);

    // LEDOUT: all 4 channels = individual PWM (10b) -> 0xAA
    i2cWrite(REG_LEDOUT, 0xAA);

    // Clear PWMs
    for (uint8_t ch=0; ch<4; ch++) i2cWrite(REG_PWM0 + ch, 0x00);

    // Group dim enabled, not blinking
    i2cWrite(REG_GRPPWM,  0xFF); // full global scale
    i2cWrite(REG_GRPFREQ, 0x00); // no blink

    // enable external buffer/driver *before* starting flare
    digitalWrite(PIN_EN, LOW); // LOW = enable
  }

  // triangle 1→100→0 across 5s
  bool runFlare(uint32_t now) {
    const uint32_t t = now - _flareStart;
    if (t >= FLARE_TOTAL_MS) { setPWM0_ifNeeded(0, now); return false; }

    uint8_t v;
    if (t < FLARE_HALF_MS) {
      // up: 1..100
      v = (uint8_t)(FLARE_MIN + (uint32_t) (FLARE_PEAK - FLARE_MIN) * t / FLARE_HALF_MS);
    } else {
      // down: 100..0
      const uint32_t td = t - FLARE_HALF_MS;
      v = (uint8_t)(FLARE_PEAK - (uint32_t) (FLARE_PEAK) * td / FLARE_HALF_MS);
    }
    setPWM0_ifNeeded(v, now);
    return true;
  }

public:
  uint16_t getId() override { return USERMOD_ID_PCA9632; }

  void setup() override {
    // status LED
    pinMode(PIN_BLINK, OUTPUT);
    digitalWrite(PIN_BLINK, LOW);
    _blinkTs = millis();

    // (Optional tiny guard if your board is touchy at boot)
     delay(1);

    // I2C
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(400000);

    // probe device
    Wire.beginTransmission(I2C_ADDR);
    _i2cOk = (Wire.endTransmission() == 0);

    if (_i2cOk) {
      chipInit();
      _flareActive = true;
      _flareStart  = millis();
      _lastPwm0    = 255;    // force first write
      _lastI2cTs   = 0;
    }
  }

  void loop() override {
    const uint32_t now = millis();

    // heartbeat (always)
    if (now - _blinkTs >= 500) {
      _blink = !_blink;
      digitalWrite(PIN_BLINK, _blink ? HIGH : LOW); // invert if your LED is active-low
      _blinkTs = now;
    }

    if (!_i2cOk) return;

    if (_flareActive) {
      _flareActive = runFlare(now);
    }
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject u = root["u"]; if (u.isNull()) u = root.createNestedObject("u");
    JsonArray arr = u.createNestedArray(F("PCA9632"));
    arr.add(_i2cOk ? F("OK") : F("I2C not found"));
    arr.add(F("GPIO2 blink 1Hz"));
    arr.add(F("PWM0 flare 1→100→0 over 5s"));
    arr.add(F("I2C 0x62 SDA=9 SCL=10; EN=5 (LOW=on)"));
    arr.add(F("MODE2: Totem+Invert, Group dim ON"));
  }

  bool readFromConfig(JsonObject&) override { return true; }
  void addToConfig(JsonObject&) override {}
};
