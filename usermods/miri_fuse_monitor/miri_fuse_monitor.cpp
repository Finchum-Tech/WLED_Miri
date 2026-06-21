#include "wled.h"

#ifdef USERMOD_MIRI_FUSE_MONITOR

#include "miri_pins.h"

#if defined(__has_include)
#if __has_include("miri_state.h")
#include "miri_state.h"
#define MIRI_FUSE_HAS_EXTERNAL_STATE 1
#endif
#endif

#ifdef ARDUINO_ARCH_ESP32
#include <driver/adc.h>
#include <esp_adc_cal.h>
#endif

#ifndef MIRI_FUSE_MONITOR_INIT_DELAY_MS
#define MIRI_FUSE_MONITOR_INIT_DELAY_MS 500U
#endif

#ifndef MIRI_FUSE_HAS_EXTERNAL_STATE
// Standalone fallback so this usermod does not depend on miri-core.
// If miri_state.h is present it is used instead of this local definition.
struct MiriStateCompat {
  uint32_t voltageIn_mV = 0;
  bool fuseBlown = false;
};
MiriStateCompat MiriState;
#endif

#ifdef MIRI_DEBUG
#define MIRI_FUSE_LOG(...) Serial.printf(__VA_ARGS__)
#else
#define MIRI_FUSE_LOG(...)
#endif

static const char MIRI_FUSE_MONITOR_PANEL_HTML[] PROGMEM = R"MIRI_FUSE_PANEL(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <style>
    body { margin: 0; background: #111; color: #e6e6e6; font: 14px Arial, sans-serif; }
    .wrap { padding: 12px; }
    .row { display: flex; align-items: center; justify-content: space-between; gap: 8px; margin-bottom: 8px; }
    .title { font-weight: 600; letter-spacing: 0.04em; }
    .badge { border-radius: 999px; padding: 2px 10px; border: 1px solid #444; }
    .ok { background: #15351b; border-color: #2f7a3d; color: #9de8ad; }
    .bad { background: #3a1515; border-color: #8d3333; color: #f5aaaa; }
    .bar { width: 100%; height: 12px; border-radius: 999px; overflow: hidden; border: 1px solid #444; background: #1f1f1f; }
    .fill { height: 100%; width: 0%; background: linear-gradient(90deg, #228be6, #2fb344); transition: width .2s; }
    .meta { margin-top: 8px; color: #b8b8b8; font-size: 12px; }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="row">
      <div class="title">Fuse Monitor</div>
      <div id="badge" class="badge ok">OK</div>
    </div>
    <div class="bar"><div id="fill" class="fill"></div></div>
    <div id="vin" class="meta">VIN: -- mV</div>
  </div>
  <script>
    const fill = document.getElementById('fill');
    const badge = document.getElementById('badge');
    const vinEl = document.getElementById('vin');
    const maxMv = 20000;
    async function refresh() {
      try {
        const resp = await fetch('/json/info');
        const json = await resp.json();
        const u = json.u || {};
        const vin = (u.Vin && u.Vin.length) ? Number(u.Vin[0]) : 0;
        const fuse = (u.Fuse && u.Fuse.length) ? String(u.Fuse[0]) : 'UNKNOWN';
        const pct = Math.max(0, Math.min(100, Math.round((vin / maxMv) * 100)));
        fill.style.width = pct + '%';
        vinEl.textContent = 'VIN: ' + vin + ' mV';
        if (fuse === 'BLOWN') {
          badge.textContent = 'BLOWN';
          badge.className = 'badge bad';
        } else {
          badge.textContent = 'OK';
          badge.className = 'badge ok';
        }
      } catch (_err) {}
    }
    refresh();
    setInterval(refresh, 500);
  </script>
</body>
</html>
)MIRI_FUSE_PANEL";

class MiriFuseMonitorUsermod : public Usermod {
private:
  static const char _name[];
  static constexpr float kDividerScale = 29.67f; // 43k/1.5k divider inverse ratio
  static constexpr uint8_t kWindowSize = 8;

  int8_t adcPin = MIRI_PIN_ADC_FUSE;
  uint32_t pollIntervalMs = 500;
  uint32_t fuseBlowThreshold_mV = 1000;
  uint32_t underVoltageThreshold_mV = 10500;
  uint32_t overVoltageThreshold_mV = 14500;
  uint8_t fuseDebounceReads = 3;
  uint32_t fuseBlowDebounceMs = 1500;

  bool adcConfigured = false;
  bool adcReady = false;
  bool pinUsesAdc2 = false;
  uint32_t adcInitAtMs = MIRI_FUSE_MONITOR_INIT_DELAY_MS;
  uint32_t lastPollMs = 0;
  uint8_t belowThresholdCount = 0;
  uint32_t belowThresholdSinceMs = 0;

  uint32_t adcWindow[kWindowSize] = {0};
  uint8_t adcWindowCount = 0;
  uint8_t adcWindowIndex = 0;

#ifdef ARDUINO_ARCH_ESP32
  bool adcCalibrationReady = false;
  adc_unit_t adcUnit = ADC_UNIT_1;
  adc1_channel_t adc1Channel = ADC1_CHANNEL_0;
  adc2_channel_t adc2Channel = ADC2_CHANNEL_0;
  esp_adc_cal_characteristics_t adcCharacteristics = {};
#endif

  inline bool shouldPoll(uint32_t now) const {
    return now - lastPollMs >= pollIntervalMs;
  }

  inline uint32_t avgAdcMilliVolts() const {
    if (adcWindowCount == 0) return 0;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < adcWindowCount; i++) sum += adcWindow[i];
    return sum / adcWindowCount;
  }

  void pushAdcSample(uint32_t mV) {
    adcWindow[adcWindowIndex] = mV;
    adcWindowIndex = (adcWindowIndex + 1) % kWindowSize;
    if (adcWindowCount < kWindowSize) adcWindowCount++;
  }

  inline bool isAdc2Blocked() const {
#ifdef ARDUINO_ARCH_ESP32
    // On classic ESP32, ADC2 reads conflict with active Wi-Fi.
    return pinUsesAdc2 && (WiFi.getMode() != WIFI_OFF);
#else
    return false;
#endif
  }

#ifdef ARDUINO_ARCH_ESP32
  bool configureAdcForPin() {
    pinMode(adcPin, INPUT);

    // Pin maps for classic ESP32 ADC channels.
    switch (adcPin) {
      case 36: adcUnit = ADC_UNIT_1; adc1Channel = ADC1_CHANNEL_0; break;
      case 37: adcUnit = ADC_UNIT_1; adc1Channel = ADC1_CHANNEL_1; break;
      case 38: adcUnit = ADC_UNIT_1; adc1Channel = ADC1_CHANNEL_2; break;
      case 39: adcUnit = ADC_UNIT_1; adc1Channel = ADC1_CHANNEL_3; break;
      case 32: adcUnit = ADC_UNIT_1; adc1Channel = ADC1_CHANNEL_4; break;
      case 33: adcUnit = ADC_UNIT_1; adc1Channel = ADC1_CHANNEL_5; break;
      case 34: adcUnit = ADC_UNIT_1; adc1Channel = ADC1_CHANNEL_6; break;
      case 35: adcUnit = ADC_UNIT_1; adc1Channel = ADC1_CHANNEL_7; break;
      case 4:  adcUnit = ADC_UNIT_2; adc2Channel = ADC2_CHANNEL_0; break;
      case 0:  adcUnit = ADC_UNIT_2; adc2Channel = ADC2_CHANNEL_1; break;
      case 2:  adcUnit = ADC_UNIT_2; adc2Channel = ADC2_CHANNEL_2; break;
      case 15: adcUnit = ADC_UNIT_2; adc2Channel = ADC2_CHANNEL_3; break;
      case 13: adcUnit = ADC_UNIT_2; adc2Channel = ADC2_CHANNEL_4; break;
      case 12: adcUnit = ADC_UNIT_2; adc2Channel = ADC2_CHANNEL_5; break;
      case 14: adcUnit = ADC_UNIT_2; adc2Channel = ADC2_CHANNEL_6; break;
      case 27: adcUnit = ADC_UNIT_2; adc2Channel = ADC2_CHANNEL_7; break;
      case 25: adcUnit = ADC_UNIT_2; adc2Channel = ADC2_CHANNEL_8; break;
      case 26: adcUnit = ADC_UNIT_2; adc2Channel = ADC2_CHANNEL_9; break;
      default:
        MIRI_FUSE_LOG("[MiriFuseMonitor] Unsupported ADC pin %d\n", adcPin);
        return false;
    }

    pinUsesAdc2 = (adcUnit == ADC_UNIT_2);
    if (adcUnit == ADC_UNIT_1) {
      adc1_config_width(ADC_WIDTH_BIT_12);
      adc1_config_channel_atten(adc1Channel, ADC_ATTEN_DB_2_5);
    } else {
      adc2_config_channel_atten(adc2Channel, ADC_ATTEN_DB_2_5);
    }

    esp_adc_cal_characterize(adcUnit, ADC_ATTEN_DB_2_5, ADC_WIDTH_BIT_12, 1100, &adcCharacteristics);
    adcCalibrationReady = true;
    MIRI_FUSE_LOG("[MiriFuseMonitor] ADC configured pin=%d unit=%d\n", adcPin, pinUsesAdc2 ? 2 : 1);
    return true;
  }

  bool readAdcMilliVolts(uint32_t& outmV) {
    if (!adcCalibrationReady) return false;
    if (adcUnit == ADC_UNIT_1) {
      int raw = adc1_get_raw(adc1Channel);
      if (raw < 0) return false;
      outmV = esp_adc_cal_raw_to_voltage(raw, &adcCharacteristics);
      return true;
    }

    int raw = 0;
    if (adc2_get_raw(adc2Channel, ADC_WIDTH_BIT_12, &raw) != ESP_OK) return false;
    outmV = esp_adc_cal_raw_to_voltage(raw, &adcCharacteristics);
    return true;
  }
#endif

  bool ensureAdcReady(uint32_t now) {
    if (adcReady) return true;
    if (!adcConfigured) {
#ifdef ARDUINO_ARCH_ESP32
      adcConfigured = configureAdcForPin();
#else
      adcConfigured = false;
#endif
      if (!adcConfigured) return false;
    }
    if (now < adcInitAtMs) return false;
    adcReady = true;
    return true;
  }

  void updateFuseState(uint32_t vin_mV, uint32_t now) {
    if (vin_mV <= fuseBlowThreshold_mV) {
      if (belowThresholdSinceMs == 0) belowThresholdSinceMs = now;
      if (belowThresholdCount < 255) belowThresholdCount++;
    } else {
      belowThresholdCount = 0;
      belowThresholdSinceMs = 0;
    }
    const uint8_t needed = (fuseDebounceReads == 0) ? 1 : fuseDebounceReads;
    const bool readsDebounced = (belowThresholdCount >= needed);
    const bool timeDebounced = (belowThresholdSinceMs > 0) && ((now - belowThresholdSinceMs) >= fuseBlowDebounceMs);
    MiriState.fuseBlown = readsDebounced && timeDebounced;
  }

public:
  void setup() override {
    server.on(F("/miri/panel/fusemonitor"), HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send_P(200, PSTR("text/html"), MIRI_FUSE_MONITOR_PANEL_HTML);
    });

    // GPIO0 note: commonly proposed for this monitor, but GPIO0 is a boot strap
    // pin and ADC2 input on classic ESP32. Reads are gated while Wi-Fi is active.
    MIRI_FUSE_LOG("[MiriFuseMonitor] setup pin=%d initDelayMs=%lu\n", adcPin, (unsigned long)adcInitAtMs);
  }

  void loop() override {
    const uint32_t now = millis();
    if (!ensureAdcReady(now)) return;
    if (!shouldPoll(now)) return;
    lastPollMs = now;

    if (isAdc2Blocked()) {
      MIRI_FUSE_LOG("[MiriFuseMonitor] ADC2 read skipped while Wi-Fi active\n");
      return;
    }

    uint32_t adc_mV = 0;
#ifdef ARDUINO_ARCH_ESP32
    if (!readAdcMilliVolts(adc_mV)) return;
#else
    return;
#endif
    pushAdcSample(adc_mV);

    const uint32_t avgAdc_mV = avgAdcMilliVolts();
    const uint32_t vin_mV = (uint32_t)lroundf((float)avgAdc_mV * kDividerScale);

    MiriState.voltageIn_mV = vin_mV;
    updateFuseState(vin_mV, now);
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray vin = user.createNestedArray(F("Vin"));
    vin.add((int)MiriState.voltageIn_mV);
    vin.add(F(" mV"));

    JsonArray fuse = user.createNestedArray(F("Fuse"));
    fuse.add(MiriState.fuseBlown ? F("BLOWN") : F("OK"));
    fuse.add(F("status"));
  }

  void addToConfig(JsonObject& root) override {
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top["pollIntervalMs"] = pollIntervalMs;
    top["fuseBlowThreshold_mV"] = fuseBlowThreshold_mV;
    top["underVoltageThreshold_mV"] = underVoltageThreshold_mV;
    top["overVoltageThreshold_mV"] = overVoltageThreshold_mV;
    top["fuseDebounceReads"] = fuseDebounceReads;
    top["fuseBlowDebounceMs"] = fuseBlowDebounceMs;
    JsonArray pinArray = top.createNestedArray("pin");
    pinArray.add(adcPin);
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root[FPSTR(_name)];
    bool configComplete = !top.isNull();

    configComplete &= getJsonValue(top["pollIntervalMs"], pollIntervalMs, 500U);
    configComplete &= getJsonValue(top["fuseBlowThreshold_mV"], fuseBlowThreshold_mV, 1000U);
    configComplete &= getJsonValue(top["underVoltageThreshold_mV"], underVoltageThreshold_mV, 10500U);
    configComplete &= getJsonValue(top["overVoltageThreshold_mV"], overVoltageThreshold_mV, 14500U);
    configComplete &= getJsonValue(top["fuseDebounceReads"], fuseDebounceReads, (uint8_t)3);
    configComplete &= getJsonValue(top["fuseBlowDebounceMs"], fuseBlowDebounceMs, 1500U);
    configComplete &= getJsonValue(top["pin"][0], adcPin, (int8_t)MIRI_PIN_ADC_FUSE);

    if (pollIntervalMs < 100U) pollIntervalMs = 100U;
    if (pollIntervalMs > 60000U) pollIntervalMs = 60000U;
    if (fuseDebounceReads == 0U) fuseDebounceReads = 1U;
    if (fuseBlowDebounceMs < pollIntervalMs) fuseBlowDebounceMs = pollIntervalMs;
    if (overVoltageThreshold_mV <= underVoltageThreshold_mV) {
      overVoltageThreshold_mV = underVoltageThreshold_mV + 1U;
    }

    return configComplete;
  }
};

const char MiriFuseMonitorUsermod::_name[] PROGMEM = "MiriFuseMonitor";

static MiriFuseMonitorUsermod miriFuseMonitorUsermod;
REGISTER_USERMOD(miriFuseMonitorUsermod);

#endif // USERMOD_MIRI_FUSE_MONITOR
