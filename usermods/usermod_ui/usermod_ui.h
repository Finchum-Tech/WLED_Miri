#pragma once

#include "wled.h"
#include "generated_ui_bundle.h"

/*
 * Usermod UI — iframe host for stock WLED UI injection (spec v0.3).
 * Target: WLED 0.15.x
 *
 * Enable:
 *   lib_deps = file://usermods/usermod_ui
 *   build_flags = -D USERMOD_UI
 */

class UsermodUI : public Usermod {
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

    uint16_t getId() override { return USERMOD_ID_UI; }

  private:
    static const char _wrapperHtml[] PROGMEM;
    static const char _injectJs[] PROGMEM;
};

const char UsermodUI::_wrapperHtml[] PROGMEM = R"=====(
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

const char UsermodUI::_injectJs[] PROGMEM = R"=====(
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

function stripPassthroughBase(url) {
  if (!url || typeof url !== 'string') return url;
  url = url.replace(/^(https?:\/\/[^/]+)\/wled_orig(?=\/)/, '$1');
  if (url.startsWith('/wled_orig/')) url = url.slice('/wled_orig'.length);
  return url;
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
    if (path === '/' || path === './') return '/wled_orig';
    let real = originalGetURL.call(iwin, path);
    real = stripPassthroughBase(real);
    if (real === '/' || real === './') return '/wled_orig';
    return real;
  };
}

function patchIframeEarly(main) {
  function tick() {
    let iwin;
    try { iwin = main.contentWindow; } catch (e) { iwin = null; }
    if (iwin && typeof iwin.getURL === 'function') interceptNavigation(iwin);
    if (!main._wledUiLoadDone) requestAnimationFrame(tick);
  }
  requestAnimationFrame(tick);
}

const mainFrame = document.getElementById('main');
patchIframeEarly(mainFrame);

mainFrame.addEventListener('load', function () {
  const idoc = this.contentDocument;
  if (!idoc) return;
  const iwin = this.contentWindow;
  interceptNavigation(iwin);
  mirrorTitleAndFavicon(idoc);
  if (window.WLEDUI && typeof window.WLEDUI._runAll === 'function') {
    window.WLEDUI._runAll(idoc);
  }
  mainFrame._wledUiLoadDone = true;
});
)=====";
