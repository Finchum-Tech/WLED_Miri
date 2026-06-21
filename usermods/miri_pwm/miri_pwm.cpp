#include "wled.h"
#include <Wire.h>

#ifdef USERMOD_MIRI_PWM
#ifndef ARDUINO_ARCH_ESP32
#error USERMOD_MIRI_PWM requires ESP32 LEDC.
#endif

#ifndef MIRI_PWM_CH_EN_PIN
#define MIRI_PWM_CH_EN_PIN -1
#endif

#ifndef MIRI_PWM_CH_EN_ACTIVE_LEVEL
#define MIRI_PWM_CH_EN_ACTIVE_LEVEL HIGH
#endif

#ifndef MIRI_PWM_I2C_SDA
#define MIRI_PWM_I2C_SDA 21
#endif

#ifndef MIRI_PWM_I2C_SCL
#define MIRI_PWM_I2C_SCL 22
#endif

#ifdef MIRI_DEBUG
#define MIRI_PWM_LOG(...) DEBUG_PRINTF(__VA_ARGS__)
#else
#define MIRI_PWM_LOG(...)
#endif

class MiriPwmUsermod : public Usermod {
private:
  enum ChannelMode : uint8_t {
    CHANNEL_MODE_PWM = 0,
    CHANNEL_MODE_STRIP = 1
  };

  enum ChannelIndex : uint8_t {
    CH_R = 0,
    CH_G = 1,
    CH_B = 2,
    CH_W = 3,
    CH_COUNT = 4
  };

  static constexpr uint8_t PWM_PINS[CH_COUNT] = {13, 12, 14, 27};
  static constexpr uint8_t SHARED_STRIP_PINS[3] = {13, 12, 14}; // R, G, B shared with strips
  static constexpr uint16_t PWM_FREQUENCY = 1220;
  static constexpr uint8_t PWM_BIT_DEPTH = 12;
  static constexpr uint32_t OWNERSHIP_REFRESH_MS = 1000;

  // PCA9633 at 0x62
  static constexpr uint8_t PCA9633_ADDR = 0x62;
  static constexpr uint8_t REG_MODE1 = 0x00;
  static constexpr uint8_t REG_MODE2 = 0x01;
  static constexpr uint8_t REG_PWM0 = 0x02;
  static constexpr uint8_t REG_LEDOUT = 0x08;

  static constexpr uint8_t V2_RESERVED_KEY_COUNT = 4;

  struct PwmChannelState {
    uint8_t mode = CHANNEL_MODE_PWM;
    bool allocated = false;
    uint8_t ledc = 255;
    uint16_t duty = 0;
  };

  bool _initialized = false;
  bool _pendingStatePush = false;
  bool _pendingPinRefresh = false;
  uint32_t _lastOwnershipRefresh = 0;

  bool _pcaPresent = false;
  bool _pcaInitialized = false;
  uint8_t _pcaLast[CH_COUNT] = {255, 255, 255, 255};

  int8_t _chEnPin = MIRI_PWM_CH_EN_PIN;
  int8_t _chEnAllocatedPin = -1;
  bool _chEnConfigured = false;
  bool _i2cPinsAllocated = false;

  uint8_t _v2ReservedColorRoles[V2_RESERVED_KEY_COUNT] = {0, 0, 0, 0};
  PwmChannelState _channels[CH_COUNT];

  static uint16_t scale12(uint8_t value, uint8_t brightness) {
    const uint16_t scaled8 = (uint16_t(value) * uint16_t(brightness) + 255U) >> 8;
    return (uint16_t(scaled8) * ((1U << PWM_BIT_DEPTH) - 1U) + 255U) >> 8;
  }

  bool isSharedStripPin(uint8_t pin) const {
    for (uint8_t i = 0; i < 3; i++) {
      if (SHARED_STRIP_PINS[i] == pin) return true;
    }
    return false;
  }

  bool isPinUsedByStripBus(uint8_t pin) const {
    for (size_t i = 0; i < BusManager::getNumBusses(); i++) {
      Bus* bus = BusManager::getBus(i);
      if (bus == nullptr || !bus->isDigital()) continue;
      uint8_t pins[OUTPUT_MAX_PINS] = {255, 255, 255, 255, 255};
      const unsigned count = bus->getPins(pins);
      for (unsigned p = 0; p < count; p++) {
        if (pins[p] == pin) return true;
      }
    }
    return false;
  }

  bool shouldUsePwm(ChannelIndex idx) const {
    if (_channels[idx].mode != CHANNEL_MODE_PWM) return false;
    const uint8_t pin = PWM_PINS[idx];
    if (!isSharedStripPin(pin)) return true;
    return !isPinUsedByStripBus(pin);
  }

  void releaseChannel(ChannelIndex idx) {
    if (!_channels[idx].allocated) return;
    const uint8_t pin = PWM_PINS[idx];
    ledcDetachPin(pin);
    if (_channels[idx].ledc != 255) {
      PinManager::deallocateLedc(_channels[idx].ledc, 1);
    }
    PinManager::deallocatePin(pin, PinOwner::UM_MIRI_PWM);
    _channels[idx].allocated = false;
    _channels[idx].ledc = 255;
    _channels[idx].duty = 0;
  }

  bool ensureChannel(ChannelIndex idx) {
    if (_channels[idx].allocated) return true;
    const uint8_t pin = PWM_PINS[idx];
    if (!PinManager::allocatePin(pin, true, PinOwner::UM_MIRI_PWM)) {
      MIRI_PWM_LOG("MiriPWM: unable to allocate GPIO%u\n", pin);
      return false;
    }

    const uint8_t ledc = PinManager::allocateLedc(1);
    if (ledc == 255) {
      PinManager::deallocatePin(pin, PinOwner::UM_MIRI_PWM);
      MIRI_PWM_LOG("MiriPWM: no LEDC channel for GPIO%u\n", pin);
      return false;
    }

    ledcSetup(ledc, PWM_FREQUENCY, PWM_BIT_DEPTH);
    ledcAttachPin(pin, ledc);
    _channels[idx].ledc = ledc;
    _channels[idx].allocated = true;
    _channels[idx].duty = 0;
    ledcWrite(ledc, 0);
    return true;
  }

  void refreshOutputOwnership() {
    for (uint8_t i = 0; i < CH_COUNT; i++) {
      const ChannelIndex idx = static_cast<ChannelIndex>(i);
      if (!shouldUsePwm(idx)) {
        releaseChannel(idx);
        continue;
      }
      ensureChannel(idx);
    }
  }

  void initPca9633() {
    managed_pin_type i2cPins[] = {
      {MIRI_PWM_I2C_SDA, true},
      {MIRI_PWM_I2C_SCL, true}
    };
    _i2cPinsAllocated = PinManager::allocateMultiplePins(i2cPins, 2, PinOwner::HW_I2C);
    if (!_i2cPinsAllocated) {
      MIRI_PWM_LOG("MiriPWM: unable to allocate I2C pins (%d,%d)\n", MIRI_PWM_I2C_SDA, MIRI_PWM_I2C_SCL);
      return;
    }

    Wire.begin(MIRI_PWM_I2C_SDA, MIRI_PWM_I2C_SCL);
    Wire.setClock(400000);

    Wire.beginTransmission(PCA9633_ADDR);
    _pcaPresent = (Wire.endTransmission() == 0);
    if (!_pcaPresent) {
      MIRI_PWM_LOG("MiriPWM: PCA9633 not detected at 0x62\n");
      return;
    }

    writePcaReg(REG_MODE1, 0x00);  // clear SLEEP
    writePcaReg(REG_MODE2, 0x04);  // totem pole, update on STOP
    writePcaReg(REG_LEDOUT, 0xAA); // PWM mode on all 4 outputs
    for (uint8_t i = 0; i < CH_COUNT; i++) {
      writePcaReg(REG_PWM0 + i, 0x00);
      _pcaLast[i] = 0;
    }
    _pcaInitialized = true;
  }

  void writePcaReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(PCA9633_ADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
  }

  void setChEnState(bool enabled) {
    if (!_chEnConfigured || _chEnPin < 0) return;
    const uint8_t active = MIRI_PWM_CH_EN_ACTIVE_LEVEL;
    digitalWrite(_chEnPin, enabled ? active : !active);
  }

  void refreshChEnOwnership() {
    if (_chEnPin == _chEnAllocatedPin) return;

    if (_chEnAllocatedPin >= 0) {
      setChEnState(false);
      PinManager::deallocatePin(_chEnAllocatedPin, PinOwner::UM_MIRI_PWM);
      _chEnAllocatedPin = -1;
      _chEnConfigured = false;
    }

    if (_chEnPin < 0) return;

    _chEnConfigured = PinManager::allocatePin(_chEnPin, true, PinOwner::UM_MIRI_PWM);
    if (!_chEnConfigured) {
      MIRI_PWM_LOG("MiriPWM: CH_EN pin allocation failed (GPIO%d)\n", _chEnPin);
      return;
    }

    _chEnAllocatedPin = _chEnPin;
    pinMode(_chEnPin, OUTPUT);
    setChEnState(true);
  }

  void applyWledState() {
    Segment& seg = strip.getMainSegment();
    const uint32_t color = seg.colors[0];
    const uint8_t brightness = bri;

    const uint8_t rgbw8[CH_COUNT] = {
      R(color), G(color), B(color), W(color)
    };

    for (uint8_t i = 0; i < CH_COUNT; i++) {
      const ChannelIndex idx = static_cast<ChannelIndex>(i);
      if (!_channels[idx].allocated || _channels[idx].ledc == 255) continue;
      const uint16_t duty = scale12(rgbw8[i], brightness);
      if (duty == _channels[idx].duty) continue;
      _channels[idx].duty = duty;
      ledcWrite(_channels[idx].ledc, duty);
    }

    if (_pcaPresent && _pcaInitialized) {
      for (uint8_t i = 0; i < CH_COUNT; i++) {
        const uint8_t scaled = (uint16_t(rgbw8[i]) * uint16_t(brightness) + 255U) >> 8;
        if (scaled == _pcaLast[i]) continue;
        writePcaReg(REG_PWM0 + i, scaled); // write-on-change only
        _pcaLast[i] = scaled;
      }
    }
  }

  static uint8_t sanitizeMode(ChannelIndex idx, uint8_t mode) {
    if (idx == CH_W) return CHANNEL_MODE_PWM;
    return (mode == CHANNEL_MODE_STRIP) ? CHANNEL_MODE_STRIP : CHANNEL_MODE_PWM;
  }

public:
  void setup() override {
    refreshChEnOwnership();
    initPca9633();
    refreshOutputOwnership();
    _lastOwnershipRefresh = millis();
    _pendingStatePush = true;
    _initialized = true;
  }

  void loop() override {
    if (!_initialized) return;

    const uint32_t now = millis();
    if (now - _lastOwnershipRefresh >= OWNERSHIP_REFRESH_MS) {
      _lastOwnershipRefresh = now;
      _pendingPinRefresh = true;
    }

    if (_pendingPinRefresh) {
      refreshChEnOwnership();
      refreshOutputOwnership();
      _pendingPinRefresh = false;
      _pendingStatePush = true;
    }
    if (_pendingStatePush) {
      applyWledState();
      _pendingStatePush = false;
    }
  }

  void onStateChange(uint8_t) override {
    _pendingStatePush = true;
  }

  void addToConfig(JsonObject& root) override {
    JsonObject top = root.createNestedObject(F("MiriPWM"));

    top[F("ch1Mode")] = _channels[CH_R].mode;
    top[F("ch2Mode")] = _channels[CH_G].mode;
    top[F("ch3Mode")] = _channels[CH_B].mode;
    top[F("ch4Mode")] = _channels[CH_W].mode;

    top[F("chEnPin")] = _chEnPin;

    // Reserved for v2 compatibility (kept but intentionally unused in v1).
    top[F("ch1ColorRole")] = _v2ReservedColorRoles[0];
    top[F("ch2ColorRole")] = _v2ReservedColorRoles[1];
    top[F("ch3ColorRole")] = _v2ReservedColorRoles[2];
    top[F("ch4ColorRole")] = _v2ReservedColorRoles[3];
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root[F("MiriPWM")];
    if (top.isNull()) return false;

    bool configComplete = true;

    uint8_t m1 = _channels[CH_R].mode;
    uint8_t m2 = _channels[CH_G].mode;
    uint8_t m3 = _channels[CH_B].mode;
    uint8_t m4 = _channels[CH_W].mode;
    int8_t chEnPin = _chEnPin;

    configComplete &= getJsonValue(top[F("ch1Mode")], m1);
    configComplete &= getJsonValue(top[F("ch2Mode")], m2);
    configComplete &= getJsonValue(top[F("ch3Mode")], m3);
    configComplete &= getJsonValue(top[F("ch4Mode")], m4);
    getJsonValue(top[F("chEnPin")], chEnPin, _chEnPin);

    _channels[CH_R].mode = sanitizeMode(CH_R, m1);
    _channels[CH_G].mode = sanitizeMode(CH_G, m2);
    _channels[CH_B].mode = sanitizeMode(CH_B, m3);
    _channels[CH_W].mode = sanitizeMode(CH_W, m4);
    _chEnPin = (chEnPin >= 0) ? chEnPin : -1;

    // Keep v2 keys in config for forward compatibility, but unused in v1 logic.
    getJsonValue(top[F("ch1ColorRole")], _v2ReservedColorRoles[0], _v2ReservedColorRoles[0]);
    getJsonValue(top[F("ch2ColorRole")], _v2ReservedColorRoles[1], _v2ReservedColorRoles[1]);
    getJsonValue(top[F("ch3ColorRole")], _v2ReservedColorRoles[2], _v2ReservedColorRoles[2]);
    getJsonValue(top[F("ch4ColorRole")], _v2ReservedColorRoles[3], _v2ReservedColorRoles[3]);

    _pendingPinRefresh = true;
    _pendingStatePush = true;
    return configComplete;
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root[F("u")];
    if (user.isNull()) user = root.createNestedObject(F("u"));
    JsonArray arr = user.createNestedArray(F("MiriPWM"));
    arr.add(F("RGBW PWM + PCA9633"));
    arr.add(_pcaPresent ? F("PCA9633:OK") : F("PCA9633:missing"));
    arr.add(F("Shared pins use strip-mode exclusion"));
  }

  uint16_t getId() override {
    return USERMOD_ID_MIRI_PWM;
  }

  ~MiriPwmUsermod() override {
    for (uint8_t i = 0; i < CH_COUNT; i++) {
      releaseChannel(static_cast<ChannelIndex>(i));
    }
    if (_chEnAllocatedPin >= 0) {
      setChEnState(false);
      PinManager::deallocatePin(_chEnAllocatedPin, PinOwner::UM_MIRI_PWM);
    }
    if (_i2cPinsAllocated) {
      uint8_t i2cPins[] = {
        static_cast<uint8_t>(MIRI_PWM_I2C_SDA),
        static_cast<uint8_t>(MIRI_PWM_I2C_SCL)
      };
      PinManager::deallocateMultiplePins(i2cPins, 2, PinOwner::HW_I2C);
      _i2cPinsAllocated = false;
    }
  }
};

static MiriPwmUsermod miriPwmUsermod;
REGISTER_USERMOD(miriPwmUsermod);

#endif // USERMOD_MIRI_PWM
