#pragma once

#include "bus_manager.h"
#include "miri_pins.h"
#include <Wire.h>
#include <string.h>

#ifdef USERMOD_MIRI

/*
 * PCA9632 (0x62) bus driver for Miri v1.0 (U11 on schematic).
 * Register-compatible with PCA9633, but PWM rates differ:
 *   PCA9632 individual mode ~1.56 kHz (this board)
 *   PCA9633 individual mode ~97 kHz
 *   Group dimming (LDR=11) adds ~190 Hz on both — avoid for coil whine.
 *
 * Appears in LED type list as PCA9633 RGBW / Ch1..Ch4 (type IDs unchanged).
 */

#ifndef MIRI_PCA9633_ADDR
#define MIRI_PCA9633_ADDR 0x62
#endif

#ifndef MIRI_PCA_RGBW32
#define MIRI_PCA_RGBW32(r,g,b,w) (uint32_t((uint8_t(w) << 24) | (uint8_t(r) << 16) | (uint8_t(g) << 8) | (uint8_t(b))))
#define MIRI_PCA_R(c) (uint8_t((c) >> 16))
#define MIRI_PCA_G(c) (uint8_t((c) >>  8))
#define MIRI_PCA_B(c) (uint8_t((c)))
#define MIRI_PCA_W(c) (uint8_t((c) >> 24))
#endif

class MiriPcaDriver {
public:
  static constexpr uint8_t REG_MODE1   = 0x00;
  static constexpr uint8_t REG_MODE2   = 0x01;
  static constexpr uint8_t REG_PWM0    = 0x02;
  static constexpr uint8_t REG_GRPPWM  = 0x06;
  static constexpr uint8_t REG_GRPFREQ = 0x07;
  static constexpr uint8_t REG_LEDOUT  = 0x08;
  static constexpr uint8_t CH_COUNT    = 4;

  struct RegSnapshot {
    bool ok = false;
    int8_t sda = -1;
    int8_t scl = -1;
    uint8_t addr = 0;
    uint8_t mode1 = 0;
    uint8_t mode2 = 0;
    uint8_t pwm[4] = {0, 0, 0, 0};
    uint8_t grpPwm = 0;
    uint8_t grpFreq = 0;
    uint8_t ledOut = 0;
  };

  static MiriPcaDriver& instance() {
    static MiriPcaDriver drv;
    return drv;
  }

  void releaseExclusiveBus() {
    if (!_i2cOwned || _i2cShared) return;
    managed_pin_type pins[] = { {_sdaPin, true}, {_sclPin, true} };
    PinManager::deallocateMultiplePins(pins, 2, PinOwner::HW_I2C);
    _i2cOwned = false;
  }

  // Open Wire on sda/scl. Returns true if the bus is usable for transactions.
  bool openBus(int8_t sda, int8_t scl) {
    if (_i2cOwned && _sdaPin == sda && _sclPin == scl) return true;
    releaseExclusiveBus();

    managed_pin_type i2cPins[] = { {sda, true}, {scl, true} };
    // Reuse if WLED (or we) already own these as HW_I2C.
    if (PinManager::getPinOwner(sda) == PinOwner::HW_I2C &&
        PinManager::getPinOwner(scl) == PinOwner::HW_I2C) {
      Wire.begin(sda, scl);
      Wire.setClock(400000);
      _sdaPin = sda;
      _sclPin = scl;
      _i2cOwned = true;
      _i2cShared = true;
      return true;
    }
    if (!PinManager::allocateMultiplePins(i2cPins, 2, PinOwner::HW_I2C)) return false;
    Wire.begin(sda, scl);
    Wire.setClock(400000);
    _sdaPin = sda;
    _sclPin = scl;
    _i2cOwned = true;
    _i2cShared = false;
    return true;
  }

  bool probeAddr(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
  }

  // Compact scan for Info: "21/22:62,48" or "21/22:-"
  void scanBus(int8_t sda, int8_t scl, char* out, size_t outLen) {
    if (!openBus(sda, scl)) {
      snprintf(out, outLen, "%d/%d:busy", (int)sda, (int)scl);
      return;
    }
    char* p = out;
    size_t left = outLen;
    int n = snprintf(p, left, "%d/%d:", (int)sda, (int)scl);
    if (n <= 0 || (size_t)n >= left) return;
    p += n; left -= n;
    bool any = false;
    for (uint8_t a = 0x08; a < 0x78; a++) {
      if (!probeAddr(a)) continue;
      n = snprintf(p, left, "%s%02X", any ? "," : "", a);
      if (n <= 0 || (size_t)n >= left) return;
      p += n; left -= n;
      any = true;
    }
    if (!any) snprintf(p, left, "-");
  }

  // PCA963x occupies 0x60..0x67 depending on A0..A2. Prefer configured addr.
  uint8_t findPcaAddr() {
    if (probeAddr(MIRI_PCA9633_ADDR)) return MIRI_PCA9633_ADDR;
    for (uint8_t a = 0x60; a <= 0x67; a++) {
      if (a == MIRI_PCA9633_ADDR) continue;
      if (probeAddr(a)) return a;
    }
    return 0;
  }

  bool ensureInit() {
    if (_ready) return true;

    const int8_t pinPairs[][2] = {
      {(int8_t)MIRI_I2C_SDA, (int8_t)MIRI_I2C_SCL},
      {21, 22},
      {9, 10}
    };
    bool found = false;
    for (uint8_t p = 0; p < 3; p++) {
      if (!openBus(pinPairs[p][0], pinPairs[p][1])) continue;
      uint8_t addr = findPcaAddr();
      if (!addr) {
        releaseExclusiveBus();
        continue;
      }
      _addr = addr;
      _pcaSda = pinPairs[p][0];
      _pcaScl = pinPairs[p][1];
      found = true;
      break;
    }
    if (!found) return false;

    writeReg(REG_MODE1, 0x00);  // wake
    // MODE2: INVRT=1 + OUTDRV=1 (0x14). Low-side FETs after 74HCT244 need INVRT
    // so PWM 0 = off. DMBLNK=0 (group-dim engine idle unless LDR=11).
    writeReg(REG_MODE2, 0x14);
    writeReg(REG_GRPPWM, 0xFF);  // ignored when LEDOUT LDR=10
    writeReg(REG_GRPFREQ, 0x00);
    // LDR=10 per channel (0xAA) = individual PWM only.
    // LDR=11 (0xFF) would AND in ~190 Hz group dim — audible + visible.
    writeReg(REG_LEDOUT, 0xAA);
    for (uint8_t ch = 0; ch < CH_COUNT; ch++) {
      writeReg(REG_PWM0 + ch, 0x00);
      _last[ch] = 0;
    }
    writeReg(REG_LEDOUT, 0xAA); // rewrite after PWM clear

    // OE is WLED relay → CH_EN → 74HCT244 1OE#/2OE# (not driven here).

    _ready = true;
    return true;
  }

  bool claim(uint8_t mask) {
    if (_claimMask & mask) return false; // channel already taken
    _claimMask |= mask;
    ensureInit(); // best-effort; bus stays valid even if chip is late
    return true;
  }

  void release(uint8_t mask) {
    _claimMask &= ~mask;
    for (uint8_t ch = 0; ch < CH_COUNT; ch++) {
      if (mask & (1u << ch)) setChannel(ch, 0);
    }
  }

  // Re-select the pins where the PCA was found (scans can move Wire elsewhere).
  bool selectPcaBus() {
    if (_pcaSda < 0 || _pcaScl < 0) return false;
    return openBus(_pcaSda, _pcaScl);
  }

  void setChannel(uint8_t ch, uint8_t value) {
    if (!_ready || ch >= CH_COUNT) return;
    if (_last[ch] == value) return;
    if (!selectPcaBus()) return;
    writeReg(REG_PWM0 + ch, value);
    _last[ch] = value;
  }

  void setRgbw(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    setChannel(0, r);
    setChannel(1, g);
    setChannel(2, b);
    setChannel(3, w);
  }

  bool ready() const { return _ready; }

  // Live chip dump (MODE1..LEDOUT). Used by Info UI /json/info.
  RegSnapshot readSnapshot() {
    RegSnapshot s;
    if (!ensureInit()) return s;
    if (!selectPcaBus()) return s;

    // Auto-increment burst: MODE1..LEDOUT (9 regs). Bit7 = AI enable in control reg.
    auto burst = [this](uint8_t* buf) -> bool {
      Wire.beginTransmission(_addr);
      Wire.write((uint8_t)(0x80 | REG_MODE1));
      if (Wire.endTransmission(false) != 0) return false;
      if (Wire.requestFrom(_addr, (uint8_t)9) != 9) return false;
      for (uint8_t i = 0; i < 9; i++) buf[i] = Wire.read();
      return true;
    };

    uint8_t buf[9];
    if (!burst(buf)) {
      // Fallback: per-register repeated-START reads.
      auto rd = [this](uint8_t reg, uint8_t& out) -> bool {
        Wire.beginTransmission(_addr);
        Wire.write(reg);
        if (Wire.endTransmission(false) != 0) return false;
        if (Wire.requestFrom(_addr, (uint8_t)1) != 1) return false;
        out = Wire.read();
        return true;
      };
      if (!rd(REG_MODE1, buf[0])) return s;
      if (!rd(REG_MODE2, buf[1])) return s;
      if (!rd(REG_PWM0 + 0, buf[2])) return s;
      if (!rd(REG_PWM0 + 1, buf[3])) return s;
      if (!rd(REG_PWM0 + 2, buf[4])) return s;
      if (!rd(REG_PWM0 + 3, buf[5])) return s;
      if (!rd(REG_GRPPWM, buf[6])) return s;
      if (!rd(REG_GRPFREQ, buf[7])) return s;
      if (!rd(REG_LEDOUT, buf[8])) return s;
    }

    s.mode1   = buf[0];
    s.mode2   = buf[1];
    s.pwm[0]  = buf[2];
    s.pwm[1]  = buf[3];
    s.pwm[2]  = buf[4];
    s.pwm[3]  = buf[5];
    s.grpPwm  = buf[6];
    s.grpFreq = buf[7];
    s.ledOut  = buf[8];
    s.sda = _pcaSda;
    s.scl = _pcaScl;
    s.addr = _addr;
    s.ok = true;
    return s;
  }

private:
  MiriPcaDriver() = default;

  void writeReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
  }

  bool _ready = false;
  bool _i2cOwned = false;
  bool _i2cShared = false;
  uint8_t _addr = MIRI_PCA9633_ADDR;
  int8_t _pcaSda = -1;
  int8_t _pcaScl = -1;
  int8_t _sdaPin = MIRI_I2C_SDA;
  int8_t _sclPin = MIRI_I2C_SCL;
  uint8_t _claimMask = 0;
  uint8_t _last[CH_COUNT] = {255, 255, 255, 255};
};


#if 0 // WLED 0.15 does not provide the Bus extension API used by BusPca
class BusPca : public Bus {
public:
  BusPca(const BusConfig& bc)
  : Bus(bc.type, bc.start, bc.autoWhite, 1, bc.reversed)
  , _claimMask(0)
  , _channel(0)
  {
    memset(_data, 0, sizeof(_data));
    if (!Bus::isPca(bc.type)) return;

    if (bc.type == TYPE_PCA9633_RGBW) {
      _claimMask = 0x0F;
      _channel = 0;
      _hasRgb = true;
      _hasWhite = true;
    } else {
      _channel = bc.type - TYPE_PCA9633_CH1; // 0..3
      if (_channel > 3) return;
      _claimMask = (uint8_t)(1u << _channel);
      _hasRgb = false;
      _hasWhite = true;
    }
    _hasCCT = false;

    if (!MiriPcaDriver::instance().claim(_claimMask)) {
      _claimMask = 0;
      return;
    }

    _valid = true;
  }

  ~BusPca() { cleanup(); }

  void setPixelColor(unsigned pix, uint32_t c) override {
    if (pix != 0 || !_valid) return;
    c = autoWhiteCalc(c);
    uint8_t r = MIRI_PCA_R(c), g = MIRI_PCA_G(c), b = MIRI_PCA_B(c), w = MIRI_PCA_W(c);
    if (_type == TYPE_PCA9633_RGBW) {
      _data[0] = r; _data[1] = g; _data[2] = b; _data[3] = w;
    } else {
      // Mono channel: use white after auto-white calc (same as PWM White).
      _data[0] = w;
    }
  }

  uint32_t getPixelColor(unsigned pix) const override {
    if (!_valid) return 0;
    if (_type == TYPE_PCA9633_RGBW) {
      return MIRI_PCA_RGBW32(_data[0], _data[1], _data[2], _data[3]);
    }
    return MIRI_PCA_RGBW32(0, 0, 0, _data[0]);
  }

  void show() override {
    if (!_valid) return;
    auto& drv = MiriPcaDriver::instance();
    if (!drv.ready() && !drv.ensureInit()) return;
    auto scale = [this](uint8_t v) -> uint8_t {
      return (uint16_t(v) * uint16_t(_bri) + 255U) >> 8;
    };

    if (_type == TYPE_PCA9633_RGBW) {
      drv.setRgbw(scale(_data[0]), scale(_data[1]), scale(_data[2]), scale(_data[3]));
    } else {
      drv.setChannel(_channel, scale(_data[0]));
    }
  }

  unsigned getPins(uint8_t* = nullptr) const override { return 0; }
  unsigned getBusSize() const override { return sizeof(BusPca); }

  void cleanup() {
    if (_claimMask) {
      MiriPcaDriver::instance().release(_claimMask);
      _claimMask = 0;
    }
    _valid = false;
  }

  static std::vector<LEDType> getLEDTypes() {
    // "V" = virtual/non-GPIO in settings UI (I2C is fixed on Miri).
    return {
      {TYPE_PCA9633_RGBW, "V", PSTR("PCA9633 RGBW")},
      {TYPE_PCA9633_CH1,  "V", PSTR("PCA9633 Ch1")},
      {TYPE_PCA9633_CH2,  "V", PSTR("PCA9633 Ch2")},
      {TYPE_PCA9633_CH3,  "V", PSTR("PCA9633 Ch3")},
      {TYPE_PCA9633_CH4,  "V", PSTR("PCA9633 Ch4")},
    };
  }

private:
  uint8_t _claimMask;
  uint8_t _channel;
  uint8_t _data[4];
};
#endif // WLED 0.15 does not provide the Bus extension API used by BusPca

#endif // USERMOD_MIRI
