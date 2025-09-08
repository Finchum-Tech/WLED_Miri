#pragma once
#include "wled.h"
#include <Wire.h>

/*
 * Usermod V2: PCA9632 (4-ch I2C LED driver)
 * - I2C: SDA=9, SCL=10, addr 0x62 (8-pin PCA9632)
 * - EN on GPIO5 (LOW = enable external buffer/driver)
 * - Outputs: Totem-pole + INVERTED (MODE2=0x14) for low-side MOSFETs
 * - Group dimming/blink registers are NOT written (we ignore GRP features)
 * - Two effect modes:
 *     1) Cosine flare on CH0 (default)
 *     2) Brownian 1-D per channel (enable with PCA_BROWNIAN_DEMO = 1)
 * - Effect values are computed into _buf[4] each loop, then scaled by WLED bri
 * - I2C writes are rate-limited to 100 tx/sec and only-on-change (burst write)
 * - GPIO2 heartbeat at 1 Hz
 *
 * Power behavior:
 *   - If bri == 0: stop effect, send 0,0,0,0 once, set EN=HIGH (disable)
 *   - If bri > 0:  run effect, scale by bri, set EN=LOW (enable)
 */

// -------- Compile-time toggle --------
#ifndef PCA_BROWNIAN_DEMO
#define PCA_BROWNIAN_DEMO 1   // set to 1 to enable Brownian; 0 = cosine flare on CH0
#endif

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
  // AI control: Auto-increment only Individual PWM registers, starting at PWM0
  static constexpr uint8_t CTRL_AI_INDIV_PWM_FROM_PWM0 = 0xA2;

  // ---- I2C rate limiting ----
  static constexpr uint32_t I2C_MIN_INTERVAL_MS = 10;     // 100 tx/s

  // ---- Cosine flare timing/shape ----
  static constexpr uint32_t FLARE_TOTAL_MS = 5000;
  static constexpr uint32_t FLARE_HALF_MS  = FLARE_TOTAL_MS / 2;
  static constexpr uint8_t  FLARE_MIN      = 1;
  static constexpr uint8_t  FLARE_PEAK     = 100;
  static constexpr uint32_t FLARE_INTERVAL_MS = 7000;

  // ---- Brownian tunables ----
  static constexpr uint32_t BROWN_DELAY_MS = 12;  // step period
  static constexpr uint8_t  BMIN_R = 0;   // PWM0
  static constexpr uint8_t  BMIN_G = 0;   // PWM1
  static constexpr uint8_t  BMIN_B = 0;   // PWM2
  static constexpr uint8_t  BMIN_W = 0;   // PWM3
  static constexpr uint8_t  BMAX_R = 255; // PWM0
  static constexpr uint8_t  BMAX_G = 255; // PWM1
  static constexpr uint8_t  BMAX_B = 255; // PWM2
  static constexpr uint8_t  BMAX_W = 255; // PWM3
  static constexpr uint8_t  BROWN_MAX_JUMP = 2;   // max hop per step

private:
  // runtime & liveness
  bool     _i2cOk = false;
  bool     _enIsLow = false;
  bool     _blink = false;
  uint32_t _blinkTs = 0;

  // computed power state (bri>0 && _i2cOk)
  bool     _powerOn = false;

  // effect state
#if PCA_BROWNIAN_DEMO
  uint32_t _brownTs = 0;
#else
  bool     _flareActive = false;
  uint32_t _flareStart  = 0;
  uint32_t _lastFlareTrigger = 0;
#endif

  // global value buffer (pre-scaled by bri before TX)
  uint8_t  _buf[4]  = {0,0,0,0};
  uint8_t  _last[4] = {255,255,255,255};   // last sent (force first write)
  uint8_t  _dirtyMask = 0x0F;              // mark all dirty at boot
  uint32_t _lastPushTs = 0;

  // write throttle helper
  uint32_t _lastI2cTs = 0;

  // --- helpers ---
  inline void enHigh() { digitalWrite(PIN_EN, HIGH); _enIsLow = false; }
  inline void enLow()  { digitalWrite(PIN_EN, LOW);  _enIsLow = true;  }

  inline bool i2cBudget(uint32_t now) {
    if (now - _lastI2cTs < I2C_MIN_INTERVAL_MS) return false;
    _lastI2cTs = now; return true;
  }

  inline void i2cWrite(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
  }

  inline void i2cWritePWMBurst(uint8_t p0, uint8_t p1, uint8_t p2, uint8_t p3) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(CTRL_AI_INDIV_PWM_FROM_PWM0); // control byte (not a register addr)
    Wire.write(p0); Wire.write(p1); Wire.write(p2); Wire.write(p3);
    Wire.endTransmission(); // OCH=0 in MODE2 => latch on STOP
  }

  void writeAllZeroes(uint32_t now) {
    // Throttle and avoid spam if already zero
    if (!i2cBudget(now)) return;
    i2cWritePWMBurst(0,0,0,0);
    for (int i=0;i<4;i++) _last[i] = 0;
    _dirtyMask = 0;
  }

  void flushIfNeeded(uint32_t now) {
    if (!_dirtyMask) return;
    if (now - _lastPushTs < I2C_MIN_INTERVAL_MS) return;

    bool any = false;
    for (int i=0;i<4;i++) if (_buf[i] != _last[i]) { any = true; break; }
    if (!any) { _dirtyMask = 0; return; }

    i2cWritePWMBurst(_buf[0], _buf[1], _buf[2], _buf[3]);
    for (int i=0;i<4;i++) _last[i] = _buf[i];
    _dirtyMask = 0;
    _lastPushTs = now;
  }

  static inline uint8_t clampU8(int v, int lo, int hi) {
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return (uint8_t)v;
  }

  void chipInit() {
    // keep buffer disabled while configuring
    pinMode(PIN_EN, OUTPUT);
    enHigh(); // HIGH = disable

    // MODE1: wake (SLEEP bit cleared). Use 0x01 (matches your known-good base).
    i2cWrite(REG_MODE1, 0x01);

    // MODE2: DMBLNK=0 (ignore group engine), INVRT=1, OUTDRV=1, OCH=0, OUTNE=00 => 0x14
    i2cWrite(REG_MODE2, 0x14);

    // LEDOUT: all 4 channels under individual PWM control -> 0xAA
    i2cWrite(REG_LEDOUT, 0xAA);

    // clear PWM channels in one burst
    i2cWritePWMBurst(0,0,0,0);
    for (int i=0;i<4;i++) _last[i] = 0;
    _dirtyMask = 0;

    // enable external buffer/driver after config
    enLow(); // LOW = enable
  }

  // ---------------------- EFFECTS ----------------------

#if PCA_BROWNIAN_DEMO
  void stepBrownianRaw(uint8_t raw[4]) {
    const uint8_t mins[4] = { BMIN_R, BMIN_G, BMIN_B, BMIN_W };
    const uint8_t maxs[4] = { BMAX_R, BMAX_G, BMAX_B, BMAX_W };
    for (uint8_t ch = 0; ch < 4; ch++) {
      int8_t d = (int8_t)random(-(int)BROWN_MAX_JUMP, (int)BROWN_MAX_JUMP + 1);
      int next = (int)raw[ch] + d;
      raw[ch] = clampU8(next, mins[ch], maxs[ch] <= 255 ? maxs[ch] : 255);
    }
  }
#else
  // Cosine ease up/down:
  // up   (u in 0..1): v = MIN + (PEAK-MIN) * 0.5 * (1 - cos(pi*u))
  // down (u in 0..1): v =       PEAK      * 0.5 * (1 + cos(pi*u))
  bool runFlareRaw(uint32_t now, uint8_t& out) {
    const uint32_t t = now - _flareStart;
    if (t >= FLARE_TOTAL_MS) { out = 0; return false; }

    if (t < FLARE_HALF_MS) {
      float u = t / (float)FLARE_HALF_MS;              // 0..1
      float eased = 0.5f * (1.0f - cosf(3.14159265f * u));
      float val = FLARE_MIN + (FLARE_PEAK - FLARE_MIN) * eased;
      out = (uint8_t)lroundf(val);
    } else {
      float u = (t - FLARE_HALF_MS) / (float)FLARE_HALF_MS;  // 0..1
      float eased = 0.5f * (1.0f + cosf(3.14159265f * u));
      float val = FLARE_PEAK * eased; // back to ~0
      out = (uint8_t)lroundf(val);
    }
    return true;
  }
#endif

  // Recompute _buf from the current effect and apply bri scaling
  void computeAndScale(uint32_t now, uint8_t briNow) {
#if PCA_BROWNIAN_DEMO
    static uint8_t raw[4] = { BMIN_R, BMIN_G, BMIN_B, BMIN_W };

    // Step at fixed cadence
    if (now - _brownTs >= BROWN_DELAY_MS) {
      _brownTs = now;
      stepBrownianRaw(raw);
    }

    // Scale by bri into _buf
    for (int i=0;i<4;i++) {
      uint16_t s = ((uint16_t)raw[i] * (uint16_t)briNow) / 255u;
      uint8_t v = (uint8_t)s;
      if (v != _buf[i]) { _buf[i] = v; _dirtyMask |= (1u<<i); }
    }
#else
    // flare on CH0 only
    uint8_t raw0;
    if (_flareActive) {
      if (!runFlareRaw(now, raw0)) {
        _flareActive = false;
        raw0 = 0;
      }
    } else if (now - _lastFlareTrigger >= FLARE_INTERVAL_MS) {
      _flareActive = true;
      _flareStart  = now;
      _lastFlareTrigger = now;
      (void)runFlareRaw(now, raw0); // compute first value
    } else {
      raw0 = 0;
    }

    // scale and store (_buf[1..3]=0)
    uint8_t scaled0 = (uint8_t)(((uint16_t)raw0 * (uint16_t)briNow) / 255u);
    if (_buf[0] != scaled0) { _buf[0] = scaled0; _dirtyMask |= (1u<<0); }
    for (int i=1;i<4;i++) {
      if (_buf[i] != 0) { _buf[i] = 0; _dirtyMask |= (1u<<i); }
    }
#endif
  }

public:
  // ---- V2 API ----
  uint16_t getId() override { return USERMOD_ID_PCA9632; }

  void setup() override {
    // heartbeat LED
    pinMode(PIN_BLINK, OUTPUT);
    digitalWrite(PIN_BLINK, LOW);
    _blinkTs = millis();

    // EN pin safe default
    pinMode(PIN_EN, OUTPUT);
    enHigh();

    // I2C
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(400000);

    // probe device
    Wire.beginTransmission(I2C_ADDR);
    _i2cOk = (Wire.endTransmission() == 0);

    if (_i2cOk) {
      chipInit();

#if PCA_BROWNIAN_DEMO
      randomSeed((uint32_t)esp_random());
      _brownTs = millis();
#else
      _flareActive      = true;
      _flareStart       = millis();
      _lastFlareTrigger = _flareStart;
#endif
      _lastI2cTs        = 0;
      // mark buffer dirty so first push happens
      _dirtyMask = 0x0F;
      for (int i=0;i<4;i++) _buf[i]=0, _last[i]=255;
    }
  }

  void loop() override {
    const uint32_t now = millis();

    // heartbeat
    if (now - _blinkTs >= 500) {
      _blink = !_blink;
      digitalWrite(PIN_BLINK, _blink ? HIGH : LOW);
      _blinkTs = now;
    }

    if (!_i2cOk) return;

    // compute power state from WLED bri
    const uint8_t briNow = bri; // 0..255
    _powerOn = (briNow > 0);

    if (!_powerOn) {
      // ensure outputs off and buffer disabled
      if (_enIsLow) enHigh();
      // zero out once (rate-limited) and bail
      writeAllZeroes(now);
      return;
    } else {
      // ensure buffer enabled
      if (!_enIsLow) enLow();
    }

    // 1) Compute effect into _buf and scale by bri
    computeAndScale(now, briNow);

    // 2) Coalesced I2C push (≤100 Hz)
    flushIfNeeded(now);
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject u = root["u"];
    if (u.isNull()) u = root.createNestedObject("u");
    JsonArray arr = u.createNestedArray(F("PCA9632"));
    arr.add(_i2cOk ? F("OK") : F("I2C not found"));
    arr.add(_enIsLow ? F("EN=LOW (enabled)") : F("EN=HIGH (disabled)"));
#if PCA_BROWNIAN_DEMO
    arr.add(F("Effect: Brownian (all 4 ch)"));
    //arr.add(F("Step=" STR(BROWN_DELAY_MS) "ms  Jump=±" STR(BROWN_MAX_JUMP)));
#else
    arr.add(F("Effect: Cosine flare (CH0)"));
    arr.add(F("5s up/down, repeats ~7s"));
#endif
    arr.add(F("Scaled by WLED bri"));
    arr.add(F("I2C 0x62 SDA=9 SCL=10; EN=5 (LOW=on)"));
    arr.add(F("MODE2: 0x14, LEDOUT: 0xAA"));
  }

  bool readFromConfig(JsonObject&) override { return true; }
  void addToConfig(JsonObject&) override {}
};

// Tiny macro-to-string helper (for JSON text above)
#ifndef STR_HELPER
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#endif
