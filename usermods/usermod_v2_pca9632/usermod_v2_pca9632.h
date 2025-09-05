#pragma once
#include "wled.h"
#include <Wire.h>

/*
 * Usermod V2: PCA9632 (4-ch) — MOSFET-friendly + Brownian demo
 * - I2C: SDA=9, SCL=10, addr 0x62
 * - EN on GPIO5 (LOW = enable)
 * - MODE2: INVRT=1, OUTDRV=1, DMBLNK=0 (group engine off)
 * - LEDOUT: 0xAA (all channels = individual PWM)
 * - Blink GPIO2 at 1 Hz so you know it's alive
 * - One-shot startup flare (CH0), then per-channel Brownian motion
 * - I2C writes capped at 100 Hz, coalesced (burst PWM0..3)
 */

#ifndef USERMOD_ID_PCA9632
#define USERMOD_ID_PCA9632 0x54
#endif

class UsermodPCA9632 : public Usermod {
public:
  // -------- Pins & I2C address --------
  static constexpr uint8_t I2C_ADDR = 0x62;
  static constexpr int MIRI_INTERNAL_SDA = 9;
  static constexpr int MIRI_INTERNAL_SCL = 10;
  static constexpr int MIRI_PWM_ENABLE  = 5;   // external buffer enable (LOW = on)
  static constexpr int MIRI_STATUS_LED  = 2;

  // -------- PCA9632 registers --------
  static constexpr uint8_t REG_MODE1   = 0x00;
  static constexpr uint8_t REG_MODE2   = 0x01;
  static constexpr uint8_t REG_PWM0    = 0x02; // ..PWM3 = 0x05
  static constexpr uint8_t REG_GRPPWM  = 0x06;
  static constexpr uint8_t REG_GRPFREQ = 0x07;
  static constexpr uint8_t REG_LEDOUT  = 0x08;

  // Control byte to start AI on PWM0 and auto-increment only PWM regs (AI=101, D=0x2)
  static constexpr uint8_t CTRL_AI_INDIV_PWM_FROM_PWM0 = 0xA2;

  // -------- I2C push rate limit --------
  static constexpr uint32_t I2C_MIN_INTERVAL_MS = 10; // 100 tx/s max

  // -------- Startup flare (CH0) --------
  static constexpr uint32_t FLARE_TOTAL_MS = 800;
  static constexpr uint32_t FLARE_HALF_MS  = FLARE_TOTAL_MS/2;
  static constexpr uint8_t  FLARE_MIN      = 0;
  static constexpr uint8_t  FLARE_PEAK     = 25;

  // -------- Brownian tunables --------
  // period between Brownian steps:
  static constexpr uint32_t BROWN_DELAY_MS = 100; 

  // per-channel minimum/maximum (clamped to [0..255] internally)
  static constexpr uint8_t BMIN_R = 0;   // PWM0
  static constexpr uint8_t BMIN_G = 0;   // PWM1
  static constexpr uint8_t BMIN_B = 0;   // PWM2
  static constexpr uint8_t BMIN_W = 0;   // PWM3

  static constexpr uint8_t BMAX_R = 255; // PWM0
  static constexpr uint8_t BMAX_G = 43; // PWM1
  static constexpr uint8_t BMAX_B = 50; // PWM2
  static constexpr uint8_t BMAX_W = 0; // PWM3

  // maximum absolute hop per step (delta in [-J..+J])
  static constexpr uint8_t BROWN_MAX_JUMP = 1;

private:
  // -------- Runtime state --------
  bool     _ok = false;        // chip present & initialized
  bool     _blink = false;
  uint32_t _blinkTs = 0;

  // PWM shadow & change tracking
  uint8_t  _pwm[4]  = {0,0,0,0};           // desired/current
  uint8_t  _last[4] = {255,255,255,255};   // last sent (force first write)
  uint8_t  _dirtyMask = 0x0F;              // bit per channel dirty at boot
  uint32_t _lastPushTs = 0;

  // flare state
  bool     _flareActive = false;
  uint32_t _flareStart  = 0;

  // Brownian state
  uint32_t _brownTs = 0;

  // --- helpers ---
  inline void i2cWrite(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
  }
  inline void i2cWritePWMBurst(uint8_t p0, uint8_t p1, uint8_t p2, uint8_t p3) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(CTRL_AI_INDIV_PWM_FROM_PWM0); // control byte (not a register addr)
    Wire.write(p0);
    Wire.write(p1);
    Wire.write(p2);
    Wire.write(p3);
    Wire.endTransmission(); // OCH=0 ⇒ latch on STOP (all 4 update together)
  }
  inline uint8_t i2cRead(uint8_t reg) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(I2C_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
  }

  // mark channel dirty if changed
  inline void setChannel(uint8_t ch, uint8_t val) {
    if (ch > 3) return;
    if (_pwm[ch] == val) return;
    _pwm[ch] = val;
    _dirtyMask |= (1u << ch);
  }

  // push all 4 in a single I2C burst (only if changed, and not more than 100/s)
  inline void flushIfNeeded(uint32_t now) {
    if (!_dirtyMask) return;
    if (now - _lastPushTs < I2C_MIN_INTERVAL_MS) return;

    bool any = false;
    for (int i=0;i<4;i++) if (_pwm[i] != _last[i]) { any = true; break; }
    if (!any) { _dirtyMask = 0; return; }

    i2cWritePWMBurst(_pwm[0], _pwm[1], _pwm[2], _pwm[3]);
    for (int i=0;i<4;i++) _last[i] = _pwm[i];
    _dirtyMask = 0;
    _lastPushTs = now;
  }

  void chipInit() {
    // keep external buffer off while configuring
    pinMode(MIRI_PWM_ENABLE, OUTPUT);
    digitalWrite(MIRI_PWM_ENABLE, HIGH); // HIGH = disable

    // MODE1: normal (osc on)
    i2cWrite(REG_MODE1, 0x00);

    // MODE2: DMBLNK=0 (no group blink), INVRT=1, OUTDRV=1, OCH=0, OUTNE=00 -> 0x14
    i2cWrite(REG_MODE2, 0x14);

    // LEDOUT: all 4 channels = individual PWM (10b per LED)
    i2cWrite(REG_LEDOUT, 0xAA);

    // Clear PWMs in one shot
    i2cWritePWMBurst(0,0,0,0);
    for (int i=0;i<4;i++) _last[i] = 0;
    _dirtyMask = 0;

    // (No group dim/blink config needed since LEDOUT=10 & DMBLNK=0)

    // enable external driver before starting flare
    digitalWrite(MIRI_PWM_ENABLE, LOW); // LOW = enable
  }

  // triangle 0→PEAK→0 across FLARE_TOTAL_MS, then stop
  bool runFlare(uint32_t now) {
    const uint32_t t = now - _flareStart;
    if (t >= FLARE_TOTAL_MS) { setChannel(0, 0); return false; }

    uint8_t v;
    if (t < FLARE_HALF_MS) {
      v = (uint8_t)(FLARE_MIN + (uint32_t)(FLARE_PEAK - FLARE_MIN) * t / FLARE_HALF_MS);
    } else {
      const uint32_t td = t - FLARE_HALF_MS;
      v = (uint8_t)(FLARE_PEAK - (uint32_t)(FLARE_PEAK) * td / FLARE_HALF_MS);
    }
    setChannel(0, v);
    return true;
  }

  // clamp helper
  static inline uint8_t clampU8(int v, int lo, int hi) {
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return (uint8_t)v;
  }

  // one Brownian step for all 4 channels
  void stepBrownian() {
    // per-channel mins/maxes
    const uint8_t mins[4] = { BMIN_R, BMIN_G, BMIN_B, BMIN_W };
    const uint8_t maxs[4] = { BMAX_R, BMAX_G, BMAX_B, BMAX_W };

    for (uint8_t ch = 0; ch < 4; ch++) {
      // random delta in [-J..+J]
      int8_t d = (int8_t)random(-(int)BROWN_MAX_JUMP, (int)BROWN_MAX_JUMP + 1);
      int next = (int)_pwm[ch] + d;
      // clamp to per-channel bounds and 0..255
      uint8_t clamped = clampU8(next, mins[ch], maxs[ch] <= 255 ? maxs[ch] : 255);
      setChannel(ch, clamped);
    }
  }

public:
  uint16_t getId() override { return USERMOD_ID_PCA9632; }

  void setup() override {
    // status LED
    pinMode(MIRI_STATUS_LED, OUTPUT);
    digitalWrite(MIRI_STATUS_LED, LOW);
    _blinkTs = millis();

    // Seed RNG (ESP32 has hardware RNG)
    randomSeed((uint32_t)esp_random());

    // I2C
    Wire.begin(MIRI_INTERNAL_SDA, MIRI_INTERNAL_SCL);
    Wire.setClock(400000);

    // probe device
    Wire.beginTransmission(I2C_ADDR);
    if (Wire.endTransmission() == 0) {
      chipInit();
      _ok = true;

      // startup flare on CH0
      _flareActive = true;
      _flareStart  = millis();

      // initialize Brownian to mins (or 0) so it grows from a defined base
      _pwm[0] = BMIN_R; _pwm[1] = BMIN_G; _pwm[2] = BMIN_B; _pwm[3] = BMIN_W;
      _dirtyMask = 0x0F; // ensure first push
      _brownTs = millis();
    } else {
      _ok = false;
    }
  }

  void loop() override {
    const uint32_t now = millis();

    // heartbeat (always)
    if (now - _blinkTs >= 500) {
      _blink = !_blink;
      digitalWrite(MIRI_STATUS_LED, _blink ? HIGH : LOW); // invert if your LED is active-low
      _blinkTs = now;
    }

    if (!_ok) return;

    // run one-shot flare first
    if (_flareActive) {
      _flareActive = runFlare(now);
    } else {
      // Brownian: step every BROWN_DELAY_MS
      if (now - _brownTs >= BROWN_DELAY_MS) {
        _brownTs = now;
        stepBrownian();
      }
    }

    // coalesced I2C push (≤100 Hz)
    flushIfNeeded(now);
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject u = root["u"]; if (u.isNull()) u = root.createNestedObject("u");
    JsonArray arr = u.createNestedArray(F("PCA9632"));
    arr.add(_ok ? F("OK") : F("I2C not found"));
    arr.add(F("GPIO2 blink 1Hz"));
    arr.add(F("Startup flare on CH0"));
    arr.add(F("Brownian per channel"));
    arr.add(F("I2C 0x62 SDA=9 SCL=10; EN=5 (LOW=on)"));
    arr.add(F("MODE2=0x14, LEDOUT=0xAA (indiv PWM)"));
  }

  bool readFromConfig(JsonObject&) override { return true; }
  void addToConfig(JsonObject&) override {}
};
