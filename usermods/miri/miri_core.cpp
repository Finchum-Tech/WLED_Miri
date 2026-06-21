#include "wled.h"

#ifdef USERMOD_MIRI

#include "miri_shared.h"
#include "miri_pins.h"

MiriState miriState;

#ifdef MIRI_DEBUG
  #define MIRI_LOG(...) Serial.printf_P(PSTR(__VA_ARGS__))
#else
  #define MIRI_LOG(...)
#endif

static const char MIRI_UI_JS[] PROGMEM = R"JS(
(() => {
  const panelRoot = document.getElementById("miri-panels");
  if (!panelRoot) return;

  const panelRoutes = [
    "tempsensor",
    "fusemonitor",
    "pwm",
    "iicie",
    "colorsphere"
  ];

  async function loadPanel(name) {
    try {
      const response = await fetch(`/miri/panel/${name}`, { cache: "no-store" });
      if (!response.ok) return;
      const html = await response.text();
      if (!html.trim()) return;
      panelRoot.insertAdjacentHTML("beforeend", html);
    } catch (_err) {
      // Panel is optional; continue loading the rest.
    }
  }

  panelRoutes.reduce(
    (promise, name) => promise.then(() => loadPanel(name)),
    Promise.resolve()
  );
})();
)JS";

static const char MIRI_BRAND_CSS[] PROGMEM = R"CSS(
:root {
  --miri-accent: #2EB8B8;
}

#miri-panels {
  margin-top: 12px;
}

.miri-panel {
  border: 1px solid rgba(46, 184, 184, 0.6);
  border-radius: 10px;
  margin: 8px 0;
  padding: 8px 10px;
  background: rgba(34, 34, 34, 0.4);
}

.miri-panel h3 {
  color: var(--miri-accent);
  margin: 0 0 6px;
  font-size: 14px;
}
)CSS";

class MiriCoreUsermod : public Usermod {
private:
  unsigned long lastAlertCheckMs = 0;
  uint32_t alertPollIntervalMs = 2000;
  bool alertLatched = false;

#ifndef WLED_DISABLE_WEBSERVER
  void registerWebRoutes() {
    server.on(F("/miri-ui.js"), HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send_P(200, PSTR("application/javascript"), MIRI_UI_JS);
    });

    server.on(F("/miri-brand.css"), HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send_P(200, PSTR("text/css"), MIRI_BRAND_CSS);
    });

    // Core-only panel route. Sub-mods should own their own /miri/panel/<name> endpoints.
    server.on(F("/miri/panel/core"), HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send_P(
        200,
        PSTR("text/html"),
        PSTR("<section class=\"miri-panel\"><h3>Miri Core</h3><p>Integration layer active.</p></section>")
      );
    });
  }
#endif

  void updateAlertState() {
    bool shouldAlert = miriState.overTemp || miriState.fuseBlown;
    miriState.alertActive = shouldAlert;
    if (!shouldAlert) {
      alertLatched = false;
      return;
    }

    if (!alertLatched) {
      alertLatched = true;
      miriState.lastAlertMs = millis();
      MIRI_LOG("[MiriCore] Alert active (overTemp=%u fuseBlown=%u)\n", miriState.overTemp, miriState.fuseBlown);
    }
  }

public:
  void setup() override {
    strlcpy(serverDescription, "Miri", sizeof(serverDescription));
#ifndef WLED_DISABLE_WEBSERVER
    registerWebRoutes();
#endif
    MIRI_LOG("[MiriCore] setup complete\n");
  }

  void loop() override {
    unsigned long now = millis();
    if (now - lastAlertCheckMs < alertPollIntervalMs) return;
    lastAlertCheckMs = now;
    updateAlertState();
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray core = user.createNestedArray(F("Miri Core"));
    core.add(F("enabled"));
    core.add(F(""));

    JsonArray alert = user.createNestedArray(F("Miri Alert"));
    alert.add(miriState.alertActive ? F("ACTIVE") : F("OK"));
    alert.add(F(""));
  }

  void addToConfig(JsonObject& root) override {
    JsonObject top = root.createNestedObject(F("MiriCore"));
    top["alertPollIntervalMs"] = alertPollIntervalMs;
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root[F("MiriCore")];
    bool configComplete = !top.isNull();
    configComplete &= getJsonValue(top["alertPollIntervalMs"], alertPollIntervalMs, 2000U);
    if (alertPollIntervalMs < 250U) alertPollIntervalMs = 250U;
    return configComplete;
  }
};

static MiriCoreUsermod miriCoreUsermod;
REGISTER_USERMOD(miriCoreUsermod);

#endif
