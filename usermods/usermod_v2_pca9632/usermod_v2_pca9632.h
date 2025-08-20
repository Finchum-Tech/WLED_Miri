#pragma once
#include "wled.h"
#include <Wire.h>

/*
 * PCA9632 usermod v2 (non-blocking, MOSFET-friendly)
 * - I2C: SDA=GPIO9, SCL=GPIO10
 * - EN buffer: GPIO5 (LOW = enable)
 * - Status LED: GPIO2 blinks @ 1 Hz
 * - Outputs: Totem-pole + INVERTED (MODE2=0x14)
 * - Group dimming enabled (DMBLNK=0 -> dimming; GRPPWM/GRPFREQ active)
 * - Startup: CH0 ramps 0 -> 100 -> 0 over 2s (one-shot)
 */

#ifndef USERMOD_ID_PCA9632
#define USERMOD_ID_PCA9632 0x13A4
#endif

class UsermodPCA9632 : public Usermod {
public:
  // ---------- pins & addr ----------
  static constexpr uint8_t I2C_ADDR = 0x62; // 1100_010b (8-pin PCA9632)
  static constexpr int PIN_SDA   = 9;
  static constexpr int PIN_SCL   = 10;
  static constexpr int PIN_EN    = 5;   // buffer enable (LOW = on)
  static constexpr int PIN_BLINK = 2;

  // ---------- PCA9632 regs ----------
  static constexpr uint8_t REG_MODE1  = 0x00;
  static constexpr uint8_t REG_MODE2  = 0x01;
  static constexpr uint8_t REG_PWM0   = 0x02; // ..PWM3 = 0x05
  static constexpr uint8_t REG_GRPPWM = 0x06;
  static constexpr uint8_t REG_GRPFREQ= 0x07;
  static constexpr uint8_t REG_LEDOUT = 0x08;

  // ---------- ramp shape ----------
  static constexpr uint32_t HALF_MS = 1000; // 1s up + 1s down = 2s total
  static constexpr uint8_t  RAMP_MAX = 100; // your request (~2/5ths)

private:
  // runtime
  bool     _i2cOk = false;
  bool     _blink = false;
  uint32_t _blinkTs = 0;

  bool     _ramping = false;
  uint32_t _rampStart = 0;

  uint8_t  _lastPWM = 255;
  uint32_t _lastWrTs = 0;

  // --- helpers ---
  inline void i2cWrite(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
  }

  void chipInit() {
    // keep external buffer off while configuring
    pinMode(PIN_EN, OUTPUT);
    digitalWrite(PIN_EN, HIGH); // HIGH = disable

    // MODE1: wake, internal osc (SLEEP=0)
    i2cWrite(REG_MODE1, 0x00);

    // MODE2: 0b0011_0100 = 0x34?  (PCA9632 bits)
    // bits: [DMBLNK=0(group dim), INVRT=1, OUTDRV=1, OCH=0(update on ACK), OUTNE=00]
    // -> INVRT(4)=1 (0x10), OUTDRV(2)=1 (0x04), DMBLNK(5)=0 -> total 0x14
    i2cWrite(REG_MODE2, 0x14);

    // LEDOUT: set all 4 channels to individual PWM (10b per LED) => 0xAA (10 10 10 10)
    i2cWrite(REG_LEDOUT, 0xAA);

    // start with all channels off
    for (uint8_t ch = 0; ch < 4; ch++) i2cWrite(REG_PWM0 + ch, 0x00);

    // group dimming defaults (full scale, steady)
    i2cWrite(REG_GRPPWM,  0xFF); // 100% group dim (acts as global scale)
    i2cWrite(REG_GRPFREQ, 0x00); // no blink

    // enable external buffer now (LOW = enable)
    digitalWrite(PIN_EN, LOW);
  }

  inline void setPWMifChanged(uint8_t ch, uint8_t v, uint32_t now) {
    if (ch > 3) return;
    // rate-limit & only-on-change (avoids I2C spam starving RTOS)
    if (v == _lastPWM && (now - _lastWrTs) < 8) return; // ~125 Hz max writes
    i2cWrite(REG_PWM0 + ch, v);
    _lastPWM = v;
    _lastWrTs = now;
  }

public:
  // ---------- V2 API ----------
  uint16_t getId() override { return USERMOD_ID_PCA9632; }

  void setup() override {
    pinMode(PIN_BLINK, OUTPUT);
    digitalWrite(PIN_BLINK, LOW);

    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(400000); // you said I2C is solid

    // probe
    Wire.beginTransmission(I2C_ADDR);
    _i2cOk = (Wire.endTransmission() == 0);

    if (_i2cOk) {
      chipInit();
      // arm one-shot ramp
      _ramping   = true;
      _rampStart = millis();
      _lastPWM   = 255;   // force first write
      _lastWrTs  = 0;
    }

    _blinkTs = millis();
  }

  void loop() override {
    const uint32_t now = millis();

    // 1) heartbeat (always, even if I2C fails)
    if (now - _blinkTs >= 500) {
      _blink = !_blink;
      digitalWrite(PIN_BLINK, _blink ? HIGH : LOW); // flip if your LED is active-low
      _blinkTs = now;
    }

    if (!_i2cOk) return;

    // 2) startup ramp on channel 0: 0→RAMP_MAX in 1s, then back to 0 in 1s, then stop
    if (_ramping) {
      const uint32_t elapsed = now - _rampStart;
      const uint32_t total   = HALF_MS * 2;

      if (elapsed >= total) {
        _ramping = false;
        setPWMifChanged(0, 0, now);
        return;
      }

      uint8_t v;
      if (elapsed < HALF_MS) {
        // up
        v = (uint8_t)((elapsed * RAMP_MAX) / HALF_MS);
      } else {
        // down
        uint32_t t = elapsed - HALF_MS;
        v = (uint8_t)(RAMP_MAX - (t * RAMP_MAX) / HALF_MS);
      }
      setPWMifChanged(0, v, now);
    }
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject u = root["u"];
    if (u.isNull()) u = root.createNestedObject("u");
    JsonArray arr = u.createNestedArray(F("PCA9632"));
    arr.add(_i2cOk ? F("OK") : F("I2C not found"));
    arr.add(F("GPIO2 blink 1Hz"));
    arr.add(F("CH0 one-shot 0↔100 over 2s"));
    arr.add(F("I2C 0x62 SDA=9 SCL=10; EN=5 (LOW=on)"));
    arr.add(F("MODE2: Totem+Invert, Group dim ON"));
  }

  bool readFromConfig(JsonObject&) override { return true; }
  void addToConfig(JsonObject&) override {}
};
