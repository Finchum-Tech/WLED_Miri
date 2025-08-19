#pragma once
#include "wled.h"
#include <Wire.h>

/*
 * Usermod: PCA9634 (8-ch I2C LED driver) - Minimal V2 mod
 * - Build-time enable with -D USERMOD_PCA9634 (see Step 3).
 * - I2C on SDA=GPIO5, SCL=GPIO10 (adjust as needed).
 * - Blinks GPIO2 @ 1 Hz so you can see the mod is running.
 * - On startup only: ramps CH0 0→~25% in 1s then back to 0 in 1s, then stops.
 * - Future: expose as a "LED type" device (placeholder hook noted below).
 */

// #ifndef USERMOD_ID_PCA9634
// #define USERMOD_ID_PCA9634 0x13A5  // arbitrary unique ID
// #endif

class UsermodPCA9634 : public Usermod {
public:
  // ---------- compile-time config ----------
  static constexpr uint8_t I2C_ADDR = 0x62;   // PCA9634 default base (A0..A2 strap). Change if needed.
  static constexpr int PIN_SDA = 9;
  static constexpr int PIN_SCL = 10;         
  static constexpr int PIN_EN = 5;            // Buffer enable LOW = ENABLE
  static constexpr int PIN_BLINK = 2;         // status LED

  // PCA9634 registers
  static constexpr uint8_t REG_MODE1   = 0x00;
  static constexpr uint8_t REG_MODE2   = 0x01;
  static constexpr uint8_t REG_PWM0    = 0x02; // PWM0..PWM3 = 0x02..0x05
  static constexpr uint8_t REG_GRPPWM  = 0x0A;
  static constexpr uint8_t REG_GRPFREQ = 0x0B;
  static constexpr uint8_t REG_LEDOUT0 = 0x0C; // LEDs 0..3
  static constexpr uint8_t REG_LEDOUT1 = 0x0D; // LEDs 4..7

  // startup ramp
  static constexpr uint32_t RAMP_HALF_PERIOD_MS = 10000; // startup time
  static constexpr uint8_t  RAMP_MAX = 100;              // ~25% of 255

private:
  bool   _ok = false;
  bool   _blinkState = false;
  uint32_t _blinkTs = 0;

  bool   _ramping = true;
  bool   _rampUp = true;
  uint32_t _rampTs = 0;

  void i2cWrite(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
  }

  void chipInit() {
    pinMode(PIN_EN, OUTPUT);
    digitalWrite(PIN_EN, HIGH);
    // MODE1: wake (SLEEP=0), use internal osc. 0x00 is fine for most cases.
    i2cWrite(REG_MODE1, 0x01);
    
    // MODE2: OUTDRV=1 (totem-pole), INVRT=0, OCH=0 (update on ACK), OUTNE=00
    // OUTDRV bit is bit2 -> value 0x04
    i2cWrite(REG_MODE2, 0x14);
    
    // LEDOUT0: set LED0..LED3 to PWM control (10b per LED) => 0b10 10 10 10 = 0xAA
    i2cWrite(REG_LEDOUT0, 0xAA);
    
    // All PWM channels start at 0
    for (uint8_t ch = 0; ch < 4; ch++) {
      i2cWrite(REG_PWM0 + ch, 0x00);
    }
    
    // No group dimming for now
    i2cWrite(REG_GRPPWM, 0x00);
    i2cWrite(REG_GRPFREQ, 0x00);

    // Turn on buffer chip LOW is ON
    digitalWrite(PIN_EN, LOW);
  }

  void setPWM(uint8_t ch, uint8_t val) {
    if (ch > 7) return;
    i2cWrite(REG_PWM0 + ch, val);
  }

public:
  // -------- V2 API ----------
  uint16_t getId() override { return USERMOD_ID_PCA9634; }

  void setup() override {
    pinMode(PIN_BLINK, OUTPUT);
    digitalWrite(PIN_BLINK, LOW);
    delay(11000);
    
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(400000); // start conservative; you can try 400k later
    
    chipInit();
    
    _ok = true;
    _blinkTs = millis();
    _rampTs = millis();
    _ramping = true;   // perform one up/down cycle on CH0, then stop
    _rampUp = true;
  }
  
  void loop() override {
    // 1) Blink GPIO2 @ 1 Hz (toggle every 500 ms)
    uint32_t now = millis();
    if (now - _blinkTs >= 500) {
      _blinkState = !_blinkState;
      digitalWrite(PIN_BLINK, _blinkState ? HIGH : LOW);
      _blinkTs = now;
    }
    
    // 2) One-time startup ramp on channel 0: 0→RAMP_MAX in 2s, then back to 0 in 2s.
    if (_ramping) {
      uint32_t elapsed = now - _rampTs;
      if (elapsed > RAMP_HALF_PERIOD_MS) {
        // switch phase
        _rampUp = !_rampUp;
        _rampTs = now;
        elapsed = 0;

        // If we just finished the down phase, stop ramping
        if (!_rampUp) {
          // we just flipped to down; allow one last 2s down
        } else {
          // flipped back to up -> that means down just completed
          // Stop after one full up+down cycle:
          _ramping = false;
          setPWM(0, 0); // ensure back at 0
          return;
        }
      }

      // compute value 0..RAMP_MAX across 2s
      uint8_t v = (uint8_t)((elapsed * RAMP_MAX) / RAMP_HALF_PERIOD_MS);
      if (!_rampUp) v = (uint8_t)(RAMP_MAX - v);
      setPWM(0, v);
    }
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject u = root[F("u")];
    if (u.isNull()) return;
    JsonArray arr = u.createNestedArray(F("PCA9634"));
    arr.add(_ok ? F("OK") : F("Not init"));
    arr.add(F("GPIO2 blinks 1Hz"));
    arr.add(F("CH0 startup ramp to 25% in 2s (once)"));
    arr.add(F("I2C addr 0x62 SDA=5 SCL=10"));
  }

  bool readFromConfig(JsonObject&) override { return true; }
  void addToConfig(JsonObject&) override {}

  // -------- Future work hook (not yet implemented) ----------
  // To appear as a "LED Type" in the UI, you’d implement a shim that
  // registers this device as a custom Bus (like a virtual segment driver),
  // mapping 4 PCA9634 channels as one logical element. That requires
  // touching WLED’s bus manager. We’ll keep this usermod clean for now.
};
