#pragma once

#include "wled.h"
#include "usermod_v2_ui_wrapper.h"

/*
 * Throwaway demo plugin for the UI wrapper registry.
 * Enable with -D USERMOD_UI_WRAPPER_DEMO (requires USERMOD_UI_WRAPPER).
 */

class UIWrapperDemoUsermod : public Usermod {
  public:
    void setup() override {
      UIWrapperUsermod::registerInjector(F("/ui_wrapper_demo.js"));

      server.on(F("/ui_wrapper_demo.js"), HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, FPSTR(CONTENT_TYPE_JAVASCRIPT), FPSTR(_demoInjectorJs));
      });
    }

    void loop() override {}

    uint16_t getId() override { return USERMOD_ID_UI_WRAPPER_DEMO; }

  private:
    static const char _demoInjectorJs[] PROGMEM;
};

const char UIWrapperDemoUsermod::_demoInjectorJs[] PROGMEM = R"=====(
window.WLEDUI.register(function (idoc) {
  if (idoc.getElementById('ui-wrapper-demo-panel')) return;

  const panel = idoc.createElement('div');
  panel.id = 'ui-wrapper-demo-panel';
  panel.textContent = 'UI wrapper demo plugin';
  panel.style.cssText = 'position:fixed;top:0;left:0;right:0;z-index:9999;padding:4px 8px;background:#222;color:#fff;font:12px sans-serif;text-align:center;pointer-events:none;opacity:0.85;';
  idoc.body.appendChild(panel);
});
)=====";
