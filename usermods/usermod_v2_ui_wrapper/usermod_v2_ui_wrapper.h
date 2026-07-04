#pragma once

#include "wled.h"
#include <vector>

/*
 * UI Wrapper usermod — iframe host for stock WLED UI injection.
 * See readme.md and wled-ui-wrapper-usermod-spec.md for architecture details.
 *
 * Enable with -D USERMOD_UI_WRAPPER in build_flags.
 * Other usermods register plugin scripts via UIWrapperUsermod::registerInjector().
 */

class UIWrapperUsermod : public Usermod {
  public:
    static void registerInjector(const String& scriptPath) {
      getInjectorList().push_back(scriptPath);
    }

    void setup() override {
      server.on(F("/"), HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebHeader *destHdr = request->getHeader(F("Sec-Fetch-Dest"));
        if (destHdr && destHdr->value() == F("iframe")) {
          serveOriginalMainUI(request);
          return;
        }
        request->send(200, FPSTR(CONTENT_TYPE_HTML), buildWrapperHtml());
      });

      server.on(F("/wled_orig"), HTTP_GET, [](AsyncWebServerRequest *request) {
        serveOriginalMainUI(request);
      });

      server.on(F("/usermod_inject.js"), HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, FPSTR(CONTENT_TYPE_JAVASCRIPT), FPSTR(_baseInjectorJs));
      });
    }

    void loop() override {}

    uint16_t getId() override { return USERMOD_ID_UI_WRAPPER; }

  private:
    static std::vector<String>& getInjectorList() {
      static std::vector<String> list;
      return list;
    }

    static String buildWrapperHtml() {
      String html = FPSTR(_wrapperHtmlHead);
      for (const String& path : getInjectorList()) {
        html += F("<script src=\"");
        html += path;
        html += F("\" defer></script>\n");
      }
      html += FPSTR(_wrapperHtmlTail);
      return html;
    }

    static const char _wrapperHtmlHead[] PROGMEM;
    static const char _wrapperHtmlTail[] PROGMEM;
    static const char _baseInjectorJs[] PROGMEM;
};

const char UIWrapperUsermod::_wrapperHtmlHead[] PROGMEM = R"=====(
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
  <script src="/usermod_inject.js" defer></script>
)=====";

const char UIWrapperUsermod::_wrapperHtmlTail[] PROGMEM = R"=====(
</body>
</html>
)=====";

const char UIWrapperUsermod::_baseInjectorJs[] PROGMEM = R"=====(
window.WLEDUI = window.WLEDUI || { _plugins: [] };

window.WLEDUI.register = function (fn) {
  window.WLEDUI._plugins.push(fn);
};

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
  iwin._navIntercepted = true;
  const originalGetURL = iwin.getURL;
  if (typeof originalGetURL !== 'function') {
    console.warn('WLEDUI: getURL() not found; home navigation may recurse into wrapper');
    return;
  }
  iwin.getURL = function (path) {
    const real = originalGetURL.call(iwin, path);
    if (path === '/' || path === './' || real === '/' || real === './') return '/wled_orig';
    return real;
  };
}

document.getElementById('main').addEventListener('load', function () {
  const idoc = this.contentDocument;
  if (!idoc) return;
  const iwin = this.contentWindow;
  mirrorTitleAndFavicon(idoc);
  interceptNavigation(iwin);
  window.WLEDUI._plugins.forEach(function (fn) {
    try { fn(idoc); } catch (e) { console.error('WLEDUI plugin failed:', e); }
  });
});
)=====";
