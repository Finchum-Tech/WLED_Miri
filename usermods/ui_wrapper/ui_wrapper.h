#pragma once

#include "wled.h"
#include "generated_ui_bundle.h"

/*
 * UI Wrapper usermod — iframe host for stock WLED UI injection (spec v0.3).
 * Target: WLED 0.15.x
 *
 * Enable:
 *   lib_deps = file://usermods/ui_wrapper
 *   build_flags = -D USERMOD_UI_WRAPPER
 *
 * Contributing usermods drop ui.js in their folder; the bundler (tools/bundle_ui.py)
 * picks them up via USERMOD_* build flags. See readme.md.
 */

class UIWrapperUsermod : public Usermod {
  public:
    void setup() override {
      server.on(F("/"), HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, FPSTR(CONTENT_TYPE_HTML), FPSTR(_wrapperHtml));
      });

      server.on(F("/wled_orig"), HTTP_GET, [](AsyncWebServerRequest *request) {
        serveOriginalMainUI(request);
      });

      server.on(F("/usermod_inject.js"), HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, FPSTR(CONTENT_TYPE_JAVASCRIPT), FPSTR(_injectJs));
      });

      server.on(F("/usermod_ui_bundle.js"), HTTP_GET, [](AsyncWebServerRequest *request) {
        handleStaticContent(request, "", 200, FPSTR(CONTENT_TYPE_JAVASCRIPT), UI_BUNDLE, UI_BUNDLE_LENGTH);
      });
    }

    void loop() override {}

    uint16_t getId() override { return USERMOD_ID_UI_WRAPPER; }

  private:
    static const char _wrapperHtml[] PROGMEM;
    static const char _injectJs[] PROGMEM;
};

const char UIWrapperUsermod::_wrapperHtml[] PROGMEM = R"=====(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <style>
    html, body { margin:0; padding:0; height:100%; overflow:hidden; }
    #main { border:0; width:100vw; height:100vh; display:block; }
  </style>
</head>
<body>
  <iframe id="main" src="/wled_orig" allow="fullscreen"></iframe>
  <script src="/usermod_ui_bundle.js"></script>
  <script src="/usermod_inject.js"></script>
</body>
</html>
)=====";

const char UIWrapperUsermod::_injectJs[] PROGMEM = R"=====(
function mirrorTitleAndFavicon(idoc) {
  document.title = idoc.title;
  const titleEl = idoc.querySelector('title');
  if (titleEl && !titleEl._mirrored) {
    titleEl._mirrored = true;
    new MutationObserver(function () { document.title = idoc.title; })
      .observe(titleEl, { childList: true, characterData: true, subtree: true });
  }
  const favEl = idoc.querySelector('link[rel~="icon"]');
  if (favEl) {
    let outerFav = document.querySelector('link[rel~="icon"]');
    if (!outerFav) {
      outerFav = document.createElement('link');
      outerFav.rel = 'icon';
      document.head.appendChild(outerFav);
    }
    outerFav.href = favEl.href;
  }
}

function interceptNavigation(iwin) {
  if (iwin._navIntercepted) return;
  const originalGetURL = iwin.getURL;
  if (typeof originalGetURL !== 'function') {
    console.warn('WLEDUI: getURL() not found; navigation interception skipped');
    return;
  }
  iwin._navIntercepted = true;
  iwin.getURL = function (path) {
    let real = originalGetURL.call(iwin, path);
    if (real.startsWith('/wled_orig/')) real = real.slice('/wled_orig'.length);
    if (real === '/' || real === './') return '/wled_orig';
    return real;
  };
}

document.getElementById('main').addEventListener('load', function () {
  const idoc = this.contentDocument;
  if (!idoc) return;
  const iwin = this.contentWindow;
  mirrorTitleAndFavicon(idoc);
  interceptNavigation(iwin);
  if (window.WLEDUI && typeof window.WLEDUI._runAll === 'function') {
    window.WLEDUI._runAll(idoc);
  }
});
)=====";
