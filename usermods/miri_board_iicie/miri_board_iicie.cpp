#include "wled.h"

#ifdef USERMOD_MIRI_BOARD_IICIE

#ifdef ARDUINO_ARCH_ESP32
#include <Wire.h>
#endif

#ifndef MIRI_LP_I2C_SDA
#define MIRI_LP_I2C_SDA 21
#endif

#ifndef MIRI_LP_I2C_SCL
#define MIRI_LP_I2C_SCL 22
#endif

#ifndef MIRI_LP_I2C_FREQ
#define MIRI_LP_I2C_FREQ 100000
#endif

#ifdef MIRI_DEBUG
#define IICIE_DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#define IICIE_DEBUG_PRINTLN(x) Serial.println(x)
#else
#define IICIE_DEBUG_PRINTF(...)
#define IICIE_DEBUG_PRINTLN(x)
#endif

class MiriBoardIICIE : public Usermod {
private:
  enum PeripheralType : uint8_t {
    PeripheralAnalog = 0,
    PeripheralDigital = 1,
    PeripheralDisplay = 2
  };

  struct PeripheralTemplate {
    PeripheralType type;
    const char* name;
    const char* description;
    const char* config;
  };

  struct MuxInstance {
    uint8_t address;
    bool present;
    bool channelSelected;
    uint8_t channel;
    String panelPath;
    String channelPath;
    String scanPath;
  };

  static constexpr uint8_t kMuxAddressCount = 9;
  static constexpr uint8_t kMuxAddresses[kMuxAddressCount] = {
    0x58, 0x59, 0x5B, 0x70, 0x71, 0x73, 0x74, 0x75, 0x77
  };

  static constexpr uint8_t kPeripheralTemplateCount = 3;
  static constexpr PeripheralTemplate kPeripheralTemplates[kPeripheralTemplateCount] = {
    {
      PeripheralAnalog,
      "AnalogPeripheral",
      "Reads analog voltage and normalizes to engineering units.",
      "{ \"name\":\"analog0\", \"channel\":0, \"addr\":\"0x48\", \"sampleMs\":250, \"scale\":1.0 }"
    },
    {
      PeripheralDigital,
      "DigitalPeripheral",
      "Controls or samples digital state from a GPIO expander.",
      "{ \"name\":\"digital0\", \"channel\":1, \"addr\":\"0x20\", \"pin\":3, \"mode\":\"input_pullup\" }"
    },
    {
      PeripheralDisplay,
      "DisplayPeripheral",
      "Pushes telemetry text to a downstream display device.",
      "{ \"name\":\"display0\", \"channel\":2, \"addr\":\"0x3C\", \"driver\":\"ssd1306\", \"rows\":4 }"
    }
  };

  MuxInstance _muxes[kMuxAddressCount];
  bool _wireReady = false;
  bool _routesReady = false;

  static const char* peripheralTypeToText(PeripheralType type) {
    switch (type) {
      case PeripheralAnalog:  return "Analog";
      case PeripheralDigital: return "Digital";
      case PeripheralDisplay: return "Display";
      default:                return "Unknown";
    }
  }

  static String formatAddress(uint8_t addr) {
    char out[7];
    snprintf(out, sizeof(out), "0x%02x", addr);
    return String(out);
  }

  MuxInstance* findMux(uint8_t addr) {
    for (uint8_t i = 0; i < kMuxAddressCount; i++) {
      if (_muxes[i].address == addr) return &_muxes[i];
    }
    return nullptr;
  }

  bool probeMuxAddress(uint8_t addr) {
#ifdef ARDUINO_ARCH_ESP32
    Wire1.beginTransmission(addr);
    return (Wire1.endTransmission() == 0);
#else
    (void)addr;
    return false;
#endif
  }

  bool writeChannelControlByte(uint8_t addr, uint8_t channel) {
    if (channel > 7) return false;
#ifndef ARDUINO_ARCH_ESP32
    (void)addr;
    return false;
#else
    const uint8_t controlByte = channel & 0x07; // PCA9849 control register B2:B0 channel select
    Wire1.beginTransmission(addr);
    Wire1.write(controlByte);
    const uint8_t result = Wire1.endTransmission();
    IICIE_DEBUG_PRINTF("[IICIE] set %s channel %u -> ctrl 0x%02X, rc=%u\r\n",
      formatAddress(addr).c_str(), channel, controlByte, result);
    return (result == 0);
#endif
  }

  String buildDrawerHtml(uint8_t addr) {
    String html;
    html.reserve(1200);
    html += F("<details open><summary><b>Peripheral Templates</b></summary>");
    html += F("<div style='padding-top:8px'>");
    html += F("<p style='margin:0 0 8px 0'>Copy a template and adjust for your downstream device.</p>");
    for (uint8_t i = 0; i < kPeripheralTemplateCount; i++) {
      const PeripheralTemplate& t = kPeripheralTemplates[i];
      html += F("<div style='border:1px solid var(--c-5);border-radius:8px;padding:8px;margin-bottom:8px'>");
      html += F("<div><b>");
      html += peripheralTypeToText(t.type);
      html += F("</b> - ");
      html += t.name;
      html += F("</div>");
      html += F("<div style='opacity:.9;margin:4px 0 6px 0'>");
      html += t.description;
      html += F("</div>");
      html += F("<textarea readonly style='width:100%;min-height:62px;font-family:monospace'>");
      html += t.config;
      html += F("</textarea>");
      html += F("<div style='font-size:.9em;padding-top:4px'>Target mux ");
      html += formatAddress(addr);
      html += F(" route: /miri/iicie/");
      html += formatAddress(addr);
      html += F("/channel</div>");
      html += F("</div>");
    }
    html += F("</div></details>");
    return html;
  }

  String buildPanelHtml(MuxInstance& mux) {
    String addr = formatAddress(mux.address);
    String html;
    html.reserve(2800);
    html += F("<section class='miri-iicie-panel' style='padding:12px;border:1px solid var(--c-5);border-radius:12px'>");
    html += F("<h3 style='margin:0 0 8px 0'>IICIe Mux ");
    html += addr;
    html += F("</h3>");
    html += F("<p style='margin:0 0 10px 0'>PCA9849 on Wire1 (SDA=");
    html += String(MIRI_LP_I2C_SDA);
    html += F(", SCL=");
    html += String(MIRI_LP_I2C_SCL);
    html += F("). Manual downstream scan only.</p>");
    html += F("<div style='display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px'>");
    for (uint8_t channel = 0; channel < 4; channel++) {
      html += F("<button class='btn' onclick='miriIicieSetChannel(\"");
      html += addr;
      html += F("\",");
      html += String(channel);
      html += F(")'>Channel ");
      html += String(channel);
      html += F("</button>");
    }
    html += F("<button class='btn' onclick='miriIicieScan(\"");
    html += addr;
    html += F("\")'>Scan Downstream</button>");
    html += F("</div>");
    html += F("<pre id='miri-iicie-result-");
    html += addr;
    html += F("' style='margin:0 0 10px 0;max-height:220px;overflow:auto'>Ready.</pre>");
    html += buildDrawerHtml(mux.address);
    html += F("</section>");
    html += F("<script>(function(){");
    html += F("if(window.miriIicieSetChannel)return;");
    html += F("window.miriIicieSetChannel=async function(addr,ch){");
    html += F("const out=document.getElementById('miri-iicie-result-'+addr);");
    html += F("const body='channel='+encodeURIComponent(ch);");
    html += F("const r=await fetch('/miri/iicie/'+addr+'/channel',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});");
    html += F("const t=await r.text(); out.textContent=t;};");
    html += F("window.miriIicieScan=async function(addr){");
    html += F("const out=document.getElementById('miri-iicie-result-'+addr);");
    html += F("const r=await fetch('/miri/iicie/'+addr+'/scan',{method:'POST'});");
    html += F("const t=await r.text(); out.textContent=t;};");
    html += F("})();</script>");
    return html;
  }

  void sendJson(AsyncWebServerRequest* request, int code, const String& payload) {
    request->send(code, "application/json", payload);
  }

  void handlePanelRequest(AsyncWebServerRequest* request, uint8_t addr) {
    MuxInstance* mux = findMux(addr);
    if (!mux || !mux->present) {
      sendJson(request, 404, F("{\"error\":\"IICIe mux not detected at requested address\"}"));
      return;
    }
    request->send(200, "text/html", buildPanelHtml(*mux));
  }

  void handleChannelRequest(AsyncWebServerRequest* request, uint8_t addr) {
    MuxInstance* mux = findMux(addr);
    if (!mux || !mux->present) {
      sendJson(request, 404, F("{\"error\":\"IICIe mux not detected at requested address\"}"));
      return;
    }

    int channel = -1;
    if (request->hasParam("channel", true)) {
      channel = request->getParam("channel", true)->value().toInt();
    } else if (request->hasParam("channel")) {
      channel = request->getParam("channel")->value().toInt();
    }

    if (channel < 0 || channel > 3) {
      sendJson(request, 400, F("{\"error\":\"channel parameter must be 0-3\"}"));
      return;
    }

    if (!writeChannelControlByte(addr, (uint8_t)channel)) {
      sendJson(request, 500, F("{\"error\":\"failed to write channel control byte\"}"));
      return;
    }

    mux->channel = (uint8_t)channel;
    mux->channelSelected = true;
    String payload = F("{\"ok\":true,\"address\":\"");
    payload += formatAddress(addr);
    payload += F("\",\"channel\":");
    payload += String(channel);
    payload += F("}");
    sendJson(request, 200, payload);
  }

  void handleScanRequest(AsyncWebServerRequest* request, uint8_t addr) {
    MuxInstance* mux = findMux(addr);
    if (!mux || !mux->present) {
      sendJson(request, 404, F("{\"error\":\"IICIe mux not detected at requested address\"}"));
      return;
    }
    if (!mux->channelSelected) {
      sendJson(request, 400, F("{\"error\":\"set channel first via /channel endpoint\"}"));
      return;
    }

    String payload = F("{\"ok\":true,\"address\":\"");
    payload += formatAddress(addr);
    payload += F("\",\"channel\":");
    payload += String(mux->channel);
    payload += F(",\"devices\":[");

    bool first = true;
    for (uint8_t device = 0x03; device <= 0x77; device++) {
#ifdef ARDUINO_ARCH_ESP32
      Wire1.beginTransmission(device);
      if (Wire1.endTransmission() == 0) {
        if (!first) payload += ',';
        payload += '\"';
        payload += formatAddress(device);
        payload += '\"';
        first = false;
      }
#endif
    }
    payload += F("]}");
    sendJson(request, 200, payload);
  }

  void registerRoutesForMux(MuxInstance& mux) {
    if (!mux.panelPath.length()) {
      const String addr = formatAddress(mux.address);
      mux.panelPath = String(F("/miri/panel/iicie/")) + addr;
      mux.channelPath = String(F("/miri/iicie/")) + addr + F("/channel");
      mux.scanPath = String(F("/miri/iicie/")) + addr + F("/scan");
    }

    server.on(mux.panelPath.c_str(), HTTP_GET, [this, addr = mux.address](AsyncWebServerRequest* request) {
      handlePanelRequest(request, addr);
    });
    server.on(mux.channelPath.c_str(), HTTP_POST, [this, addr = mux.address](AsyncWebServerRequest* request) {
      handleChannelRequest(request, addr);
    });
    server.on(mux.scanPath.c_str(), HTTP_POST, [this, addr = mux.address](AsyncWebServerRequest* request) {
      handleScanRequest(request, addr);
    });
  }

  void detectMuxes() {
    for (uint8_t i = 0; i < kMuxAddressCount; i++) {
      _muxes[i].address = kMuxAddresses[i];
      _muxes[i].present = probeMuxAddress(kMuxAddresses[i]);
      _muxes[i].channelSelected = false;
      _muxes[i].channel = 0;
      _muxes[i].panelPath = "";
      _muxes[i].channelPath = "";
      _muxes[i].scanPath = "";
      if (_muxes[i].present) {
        IICIE_DEBUG_PRINTF("[IICIE] mux detected at %s\r\n", formatAddress(kMuxAddresses[i]).c_str());
      }
    }
  }

public:
  void setup() override {
#ifndef ARDUINO_ARCH_ESP32
    IICIE_DEBUG_PRINTLN("[IICIE] USERMOD_MIRI_BOARD_IICIE requires ESP32 Wire1 support.");
    return;
#else
    Wire1.begin(MIRI_LP_I2C_SDA, MIRI_LP_I2C_SCL, MIRI_LP_I2C_FREQ);
    _wireReady = true;
    detectMuxes();
    for (uint8_t i = 0; i < kMuxAddressCount; i++) {
      if (_muxes[i].present) {
        registerRoutesForMux(_muxes[i]);
      }
    }
    _routesReady = true;
    IICIE_DEBUG_PRINTLN("[IICIE] setup complete.");
#endif
  }

  void loop() override {}

  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");
    JsonArray iicie = user["IICIE"];
    if (iicie.isNull()) iicie = user.createNestedArray("IICIE");
    else iicie.clear();
    uint8_t activeMuxes = 0;
    for (uint8_t i = 0; i < kMuxAddressCount; i++) {
      if (_muxes[i].present) activeMuxes++;
    }
    iicie.add(activeMuxes);
    iicie.add(_wireReady && _routesReady ? F(" mux online") : F(" mux pending"));
  }
};

constexpr uint8_t MiriBoardIICIE::kMuxAddresses[kMuxAddressCount];
constexpr MiriBoardIICIE::PeripheralTemplate MiriBoardIICIE::kPeripheralTemplates[kPeripheralTemplateCount];

static MiriBoardIICIE miriBoardIicieUsermod;
REGISTER_USERMOD(miriBoardIicieUsermod);

#endif
