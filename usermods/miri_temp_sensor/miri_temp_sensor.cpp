#include "wled.h"
#include <Wire.h>

#ifdef USERMOD_MIRI_TEMP_SENSOR

#if defined(__has_include) && __has_include("miri_pins.h")
#include "miri_pins.h"
#elif defined(__has_include) && __has_include("miri/miri_pins.h")
#include "miri/miri_pins.h"
#endif

#ifndef MIRI_I2C_SDA
#define MIRI_I2C_SDA 21
#endif

#ifndef MIRI_I2C_SCL
#define MIRI_I2C_SCL 22
#endif

#if defined(__has_include) && __has_include("miri_shared.h")
#include "miri_shared.h"
#elif defined(__has_include) && __has_include("miri/miri_shared.h")
#include "miri/miri_shared.h"
#else
struct MiriStateShim {
  float tempCelsius = NAN;
  bool overTemp = false;
};
static MiriStateShim MiriState;
#endif

class MiriTempSensorUsermod : public Usermod {
private:
  static constexpr uint8_t TMP1075_ADDR = 0x49;
  static constexpr uint8_t TMP1075_TEMP_REG = 0x00;
  static constexpr float TMP1075_LSB_C = 0.0625f;
  static constexpr uint32_t MIN_POLL_INTERVAL_MS = 250;
  static constexpr float ALERT_HYSTERESIS_C = 2.0f;

  uint32_t pollIntervalMs = 2000;
  float overTempThresholdC = 70.0f;
  uint32_t lastPollMs = 0;
  bool sensorPresent = false;
  bool hadValidRead = false;
  float lastTempC = NAN;

  static const char _name[];
  static const char _pollIntervalMs[];
  static const char _overTempThresholdC[];

  bool probeSensor() {
    Wire.beginTransmission(TMP1075_ADDR);
    return (Wire.endTransmission() == 0);
  }

  bool readTemperature(float& outCelsius) {
    Wire.beginTransmission(TMP1075_ADDR);
    Wire.write(TMP1075_TEMP_REG);
    if (Wire.endTransmission(false) != 0) return false;

    const uint8_t expectedBytes = 2;
    if (Wire.requestFrom((int)TMP1075_ADDR, (int)expectedBytes) != expectedBytes) return false;

    const uint8_t msb = Wire.read();
    const uint8_t lsb = Wire.read();
    int16_t raw = (int16_t)((msb << 8) | lsb);
    raw >>= 4;
    if (raw & 0x0800) raw |= 0xF000;  // sign-extend TMP1075 12-bit value

    outCelsius = raw * TMP1075_LSB_C;
    return true;
  }

  void updateOverTempState(float celsius) {
    if (celsius > overTempThresholdC) {
      MiriState.overTemp = true;
      return;
    }

    if (celsius <= (overTempThresholdC - ALERT_HYSTERESIS_C)) {
      MiriState.overTemp = false;
    }
  }

  void pollSensor() {
    float readingC = NAN;
    if (!readTemperature(readingC)) {
      sensorPresent = false;
      hadValidRead = false;
#ifdef MIRI_DEBUG
      Serial.println(F("[MiriTempSensor] TMP1075 read failed."));
#endif
      return;
    }

    sensorPresent = true;
    hadValidRead = true;
    lastTempC = readingC;
    MiriState.tempCelsius = readingC;
    updateOverTempState(readingC);
  }

public:
  void setup() override {
#ifdef ESP8266
    Wire.begin();
#else
    Wire.begin(MIRI_I2C_SDA, MIRI_I2C_SCL);
#endif

    sensorPresent = probeSensor();
    MiriState.overTemp = false;
    MiriState.tempCelsius = NAN;

#ifdef MIRI_DEBUG
    if (sensorPresent) {
      Serial.println(F("[MiriTempSensor] TMP1075 detected at 0x49."));
    } else {
      Serial.println(F("[MiriTempSensor] TMP1075 not responding at 0x49."));
    }
#endif

    server.on(F("/miri/panel/tempsensor"), HTTP_GET, [](AsyncWebServerRequest* request) {
      static const char panelHtml[] PROGMEM = R"HTML(
<div class='miri-panel' id='miri-tempsensor-panel'>
  <h3>Board Temp</h3>
  <div id='miri-tempsensor-value'>--.- C</div>
  <div id='miri-tempsensor-status'>UNKNOWN</div>
</div>
<script>
(function(){
  const valueEl = document.getElementById('miri-tempsensor-value');
  const statusEl = document.getElementById('miri-tempsensor-status');
  if (!valueEl || !statusEl) return;

  async function updateTempPanel() {
    try {
      const r = await fetch('/json/info');
      if (!r.ok) return;
      const info = await r.json();
      const user = info && info.u ? info.u : {};
      const board = user['Board Temp'];
      const over = user['Temp Alert'];
      if (Array.isArray(board) && board.length > 0) {
        const unit = board.length > 1 ? board[1] : '';
        valueEl.textContent = board[0] + unit;
      }
      statusEl.textContent = Array.isArray(over) ? over[0] : 'OK';
    } catch (_) {}
  }

  updateTempPanel();
  setInterval(updateTempPanel, 2000);
})();
</script>
      )HTML";
      request->send_P(200, PSTR("text/html"), panelHtml);
    });
  }

  void loop() override {
    if (millis() - lastPollMs < pollIntervalMs) return;
    lastPollMs = millis();
    pollSensor();
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray boardTemp = user.createNestedArray(F("Board Temp"));
    if (hadValidRead) {
      boardTemp.add(lastTempC);
      boardTemp.add(F(" C"));
    } else if (sensorPresent) {
      boardTemp.add(F("--.-"));
      boardTemp.add(F(" C"));
    } else {
      boardTemp.add(F("N/A"));
      boardTemp.add(F(" sensor"));
    }

    if (MiriState.overTemp) {
      JsonArray alert = user.createNestedArray(F("Temp Alert"));
      alert.add(F("OVER TEMP"));
      alert.add(F("!"));
    }
  }

  void addToConfig(JsonObject& root) override {
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top[FPSTR(_pollIntervalMs)] = pollIntervalMs;
    top[FPSTR(_overTempThresholdC)] = overTempThresholdC;
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root[FPSTR(_name)];
    if (top.isNull()) return false;

    getJsonValue(top[FPSTR(_pollIntervalMs)], pollIntervalMs);
    getJsonValue(top[FPSTR(_overTempThresholdC)], overTempThresholdC);
    pollIntervalMs = max(pollIntervalMs, MIN_POLL_INTERVAL_MS);
    return true;
  }

  void appendConfigData() override {
    oappend(F("addInfo('Miri Temp Sensor:pollIntervalMs', 1, 'ms (>=250)');"));
    oappend(F("addInfo('Miri Temp Sensor:overTempThreshold_C', 1, '&deg;C (2C hysteresis)');"));
  }
};

const char MiriTempSensorUsermod::_name[] PROGMEM = "Miri Temp Sensor";
const char MiriTempSensorUsermod::_pollIntervalMs[] PROGMEM = "pollIntervalMs";
const char MiriTempSensorUsermod::_overTempThresholdC[] PROGMEM = "overTempThreshold_C";

static MiriTempSensorUsermod miriTempSensorUsermod;
REGISTER_USERMOD(miriTempSensorUsermod);

#endif
