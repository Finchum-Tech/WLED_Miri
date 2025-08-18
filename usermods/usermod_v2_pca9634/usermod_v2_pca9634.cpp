#include "wled.h"
#include <Wire.h>

/*
 * Usermod V2: PCA9634 (8-ch I2C LED driver)
 * - Enable with -D USERMOD_PCA9634 and add folder name to 'custom_usermods' in platformio.ini
 * - I2C: SDA=GPIO5, SCL=GPIO10   (verify GPIO10 is really free on your ESP32-MINI-1)
 * - Blinks GPIO2 (1 Hz) as a heartbeat
 * - Startup only: CH0 0→~25% in 2s, then back to 0 in 2s, then idle
 */

#ifndef USERMOD_ID_PCA9634
#define USERMOD_ID_PCA9634 0x13A5
#endif

class UsermodPCA9634 : public Usermod {
  // ----- Compile-time config -----
  static constexpr uint8_t  I2C_ADDR = 0x70;     // adjust if A0..A2 strapped differently
  static constexpr int      PIN_SDA  = 9;
  static constexpr int      PIN_SCL  = 10;       // NOTE: often tied to flash on some modules
  static constexpr int      PIN_BLINK= 2;

  // Registers
  static constexpr uint8_t REG_MODE1   = 0x00;
  static constexpr uint8_t REG_MODE2   = 0x01;
  static constexpr uint8_t REG_PWM0    = 0x02;   // ..+7 up to 0x09
  static constexpr uint8_t REG_GRPPWM  = 0x0A;
  static constexpr uint8_t REG_GRPFREQ = 0x0B;
  static constexpr uint8_t REG_LEDOUT0 = 0x0C;   // LEDs 0..3
  static constexpr uint8_t REG_LEDOUT1 = 0x0D;   // LEDs 4..7

  // Startup ramp
  static constexpr uint32_t RAMP_HALF_MS = 2000; // 2s up, 2s down
  static constexpr uint8_t  RAMP_MAX     = 63;   // ~25%

  // State
  bool _ok = false;
  bool _blink = false;
  uint32_t _blinkTs = 0;

  bool _ramping = true;
  bool _rampUp = true;
  uint32_t _rampTs = 0;

  // I2C helpers
  void i2cWrite(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
  }
  void setPWM(uint8_t ch, uint8_t val) {
    if (ch < 8) i2cWrite(REG_PWM0 + ch, val);
  }

  void chipInit() {
    // MODE1: wake (SLEEP=0), internal osc
    i2cWrite(REG_MODE1, 0x00);
    // MODE2: OUTDRV=1 (totem-pole), OCH=0 (update on ACK), INVRT=0, OUTNE=00
    i2cWrite(REG_MODE2, 0x04);
    // LEDOUT0: LED0..3 to PWM (10b each) = 0xAA; LEDOUT1: keep 4..7 off for now
    i2cWrite(REG_LEDOUT0, 0xAA);
    i2cWrite(REG_LEDOUT1, 0x00);
    // All PWMs = 0
    for (uint8_t i=0;i<8;i++) setPWM(i, 0);
    // No group dimming initially
    i2cWrite(REG_GRPPWM, 0x00);
    i2cWrite(REG_GRPFREQ, 0x00);
  }

public:
  uint16_t getId() override { return USERMOD_ID_PCA9634; }

  void setup() override {
    pinMode(PIN_BLINK, OUTPUT);
    digitalWrite(PIN_BLINK, LOW);

    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(100000); // conservative for bring-up
    chipInit();

    _ok = true;
    _blinkTs = millis();
    _rampTs  = millis();
    _ramping = true;
    _rampUp  = true;
  }

  void loop() override {
    // Always blink, even if _ok is false (so you can see the mod is alive)
  uint32_t now = millis();
  if (now - _blinkTs >= 500) {
    _blink = !_blink;
    digitalWrite(PIN_BLINK, _blink ? HIGH : LOW);
    _blinkTs = now;
  }
    
    if (!_ok) return;
    uint32_t now = millis();

    // Startup ramp on CH0 (once)
    if (_ramping) {
      uint32_t elapsed = now - _rampTs;
      if (elapsed > RAMP_HALF_MS) {
        _rampUp = !_rampUp;
        _rampTs = now;
        elapsed = 0;

        // completed down phase -> stop
        if (_rampUp) { // we just flipped back to up => down finished
          _ramping = false;
          setPWM(0, 0);
          return;
        }
      }
      uint8_t v = (uint8_t)((elapsed * RAMP_MAX) / RAMP_HALF_MS);
      if (!_rampUp) v = (uint8_t)(RAMP_MAX - v);
      setPWM(0, v);
    }
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject u = root["u"];
    if (u.isNull()) return;
    JsonArray arr = u.createNestedArray(F("PCA9634"));
    arr.add(_ok ? F("OK") : F("Not init"));
    arr.add(F("GPIO2 blink 1Hz"));
    arr.add(F("CH0 startup ramp 0↔25% (2s half cycle)"));
    arr.add(F("I2C 0x70 SDA=5 SCL=10"));
  }

  bool readFromConfig(JsonObject&) override { return true; }
  void addToConfig(JsonObject&) override {}
};
