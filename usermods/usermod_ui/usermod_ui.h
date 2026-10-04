#pragma once

#include "wled.h"
#include "html_ui.h"  // PAGE_index / PAGE_index_L
#include "generated_ui_bundle.h"

/*
 * Usermod UI: iframe host for stock WLED UI injection (spec v0.3).
 *
 * WLED v0.15.x setup:
 *   lib_deps = file://usermods/usermod_ui
 *   build_flags = -D USERMOD_UI
 *
 * WLED v0.16.0 and later:
 *   custom_usermods = usermod_ui
 */

class UsermodUI : public Usermod {
  public:
    void setup() override {
      server.on(F("/"), HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, FPSTR(CONTENT_TYPE_HTML), FPSTR(_wrapperHtml));
      });

      // Same stock main-page serve path as core's GET / (captive / welcome / PAGE_index).
      server.on(F("/wled_orig"), HTTP_GET, [](AsyncWebServerRequest *request) {
        if (captivePortal(request)) return;
        if (!showWelcomePage || request->hasArg(F("sliders"))) {
          handleStaticContent(request, F("/index.htm"), 200, FPSTR(CONTENT_TYPE_HTML), PAGE_index, PAGE_index_L);
        } else {
          serveSettings(request);
        }
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
  <iframe id="main" allow="fullscreen"></iframe>
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
  if (iwin._navIntercepted) return true;
  const originalGetURL = iwin.getURL;
  if (typeof originalGetURL !== 'function') {
    return false;
  }
  iwin._navIntercepted = true;
  try {
    iwin.loc = false;
    iwin.locproto = iwin.location.protocol;
    iwin.locip = iwin.location.hostname + (iwin.location.port ? ':' + iwin.location.port : '');
  } catch (e) {}
  iwin.getURL = function (path) {
    if (path === '/' || path === './') return '/wled_orig';
    let real = originalGetURL.call(iwin, path);
    real = stripPassthroughBase(real);
    if (real === '/' || real === './') return '/wled_orig';
    return real;
  };
  return true;
}

function ensureTabLayout(idoc) {
  const iwin = idoc.defaultView;
  if (!iwin) return;
  window.WLEDUI = window.WLEDUI || {};

  window.WLEDUI = window.WLEDUI || {};
  // PC mode: show all tabs side-by-side only when count <= this.
  // Above that, use a peek carousel (PC_PEEK_COLS visible) so a half-column hints to swipe.
  window.WLEDUI.PC_MULTI_COL_MAX = 5;
  window.WLEDUI.PC_PEEK_COLS = 5.5;

  if (!window.WLEDUI.getTabCount) {
    window.WLEDUI.getTabCount = function (doc) {
      const tabBar = doc && doc.getElementById('bot');
      return tabBar ? tabBar.querySelectorAll('button.tablinks').length : 0;
    };
  }

  if (!window.WLEDUI.carouselMode) {
    window.WLEDUI.carouselMode = function (doc) {
      const win = doc && doc.defaultView;
      if (!win || win.simplifiedUI) return false;
      if (!win.pcMode) return true;
      return window.WLEDUI.getTabCount(doc) > window.WLEDUI.PC_MULTI_COL_MAX;
    };
  }

  if (!window.WLEDUI.pcShowsAllColumns) {
    window.WLEDUI.pcShowsAllColumns = function (doc) {
      const win = doc && doc.defaultView;
      return !!(win && win.pcMode && !win.simplifiedUI &&
        window.WLEDUI.getTabCount(doc) <= window.WLEDUI.PC_MULTI_COL_MAX);
    };
  }

  if (!window.WLEDUI.pcPeekCarousel) {
    window.WLEDUI.pcPeekCarousel = function (doc) {
      const win = doc && doc.defaultView;
      return !!(win && win.pcMode && !win.simplifiedUI && window.WLEDUI.carouselMode(doc));
    };
  }

  if (!window.WLEDUI.pcPeekMaxIndex) {
    window.WLEDUI.pcPeekMaxIndex = function (doc) {
      const n = window.WLEDUI.getTabCount(doc) || 1;
      const peek = window.WLEDUI.PC_PEEK_COLS || 5.5;
      return Math.max(0, Math.ceil(n - peek));
    };
  }

  if (!window.WLEDUI.syncTabLayout) {
    window.WLEDUI.syncTabLayout = function (doc) {
      const win = doc.defaultView;
      const tabHost = doc.querySelector('.container');
      const tabBar = doc.getElementById('bot');
      if (!tabHost || !tabBar || !win) return;
      const n = window.WLEDUI.getTabCount(doc);
      const carousel = window.WLEDUI.carouselMode(doc);
      const peek = window.WLEDUI.pcPeekCarousel(doc);
      const manyTabs = n > window.WLEDUI.PC_MULTI_COL_MAX;
      const peekCols = window.WLEDUI.PC_PEEK_COLS;
      tabHost.style.setProperty('--n', n);
      tabHost.style.setProperty('--tab-count', String(n));
      tabHost.style.setProperty('--visible', String(peekCols));
      tabHost.classList.toggle('wled-ui-carousel', carousel && !peek);
      tabHost.classList.toggle('wled-ui-pc-peek', peek);
      tabBar.classList.toggle('wled-ui-many-tabs', manyTabs);
      if (peek) {
        // n tabs, each viewport/peekCols wide; half column peeks the next tab.
        tabHost.style.width = ((n / peekCols) * 100) + '%';
        tabHost.style.setProperty('--i', tabHost.style.getPropertyValue('--i') || '0');
      } else if (carousel || (!win.pcMode && !win.simplifiedUI)) {
        tabHost.style.width = (n * 100) + '%';
      } else {
        // PC mode, tabs all fit: stock-style multi-column row.
        tabHost.style.width = '100%';
        tabHost.style.setProperty('--i', '0');
      }
      tabBar.style.setProperty('--tab-count', String(n));
      win._wledUiTabCount = n;
    };
  }

  if (!window.WLEDUI.scrollTabIntoView) {
    window.WLEDUI.scrollTabIntoView = function (doc, tabI) {
      const tabBar = doc && doc.getElementById('bot');
      if (!tabBar || !tabBar.classList.contains('wled-ui-many-tabs')) return;
      const btn = tabBar.querySelectorAll('button.tablinks')[tabI];
      if (btn && btn.scrollIntoView) {
        btn.scrollIntoView({ behavior: 'smooth', inline: 'center', block: 'nearest' });
      }
    };
  }

  if (!idoc.getElementById('usermod-ui-tab-layout-style')) {
    const style = idoc.createElement('style');
    style.id = 'usermod-ui-tab-layout-style';
    style.textContent =
      '#bot.bot button.tablinks{width:calc(100%/var(--tab-count,4));}' +
      '#bot.bot.wled-ui-many-tabs{overflow-x:auto;overflow-y:hidden;-webkit-overflow-scrolling:touch;' +
      'display:flex;flex-wrap:nowrap;scrollbar-width:thin;}' +
      '#bot.bot.wled-ui-many-tabs button.tablinks{float:none;flex:0 0 auto;width:auto;min-width:3.25em;' +
      'max-width:8em;padding-left:6px;padding-right:6px;}' +
      '#bot.bot.wled-ui-many-tabs button.tablinks .tab-label{overflow:hidden;text-overflow:ellipsis;' +
      'white-space:nowrap;max-width:6em;}' +
      /* PC peek: show ~5.5 columns; translate by one tab (1/tab-count of strip). */
      '.container.wled-ui-pc-peek{' +
        'transform:translate(calc(var(--i,0)/var(--tab-count,1)*-100%));' +
      '}' +
      '.container.wled-ui-pc-peek .tabcontent{' +
        'width:calc(100%/var(--tab-count,1));' +
      '}';
    idoc.head.appendChild(style);
  }

  if (!iwin._wledUiTabLayoutResize) {
    iwin._wledUiTabLayoutResize = true;
    iwin.addEventListener('resize', function () {
      if (window.WLEDUI && typeof window.WLEDUI.syncTabLayout === 'function') {
        window.WLEDUI.syncTabLayout(idoc);
      }
    });
  }

  if (!iwin._wledUiTabLayoutHooks) {
    iwin._wledUiTabLayoutHooks = true;
    function resyncTabLayout() {
      if (window.WLEDUI && typeof window.WLEDUI.syncTabLayout === 'function') {
        window.WLEDUI.syncTabLayout(idoc);
      }
    }
    if (typeof iwin.togglePcMode === 'function' && !iwin.togglePcMode._wledUiSync) {
      const origTogglePcMode = iwin.togglePcMode;
      iwin.togglePcMode = function (fromB) {
        origTogglePcMode.apply(iwin, arguments);
        resyncTabLayout();
      };
      iwin.togglePcMode._wledUiSync = true;
    }
    if (typeof iwin.size === 'function' && !iwin.size._wledUiSync) {
      const origSize = iwin.size;
      iwin.size = function () {
        origSize.apply(iwin, arguments);
        resyncTabLayout();
      };
      iwin.size._wledUiSync = true;
    }
  }

  window.WLEDUI.isMainIndexPage = function (doc) {
    return !!(doc && doc.getElementById('Colors'));
  };

  installCarouselExtensions(idoc);
}

function installCarouselExtensions(idoc) {
  const iwin = idoc.defaultView;
  const tabHost = idoc.querySelector('.container');
  if (!iwin || !tabHost || !window.WLEDUI.isMainIndexPage(idoc)) return;

  if (typeof iwin.openTab === 'function' && !iwin.openTab._wledUiHost) {
    const origOpenTab = iwin.openTab;
    iwin.openTab = function (tabI, force) {
      const carousel = window.WLEDUI.carouselMode(idoc);
      const allCols = window.WLEDUI.pcShowsAllColumns(idoc);
      const peek = window.WLEDUI.pcPeekCarousel(idoc);
      // Stock PC with all tabs visible: do not slide (--i stays 0).
      if (allCols && !force) {
        if (typeof iwin.updateTablinks === 'function') iwin.updateTablinks(tabI);
        return;
      }
      if (carousel || peek) {
        let i = tabI;
        if (peek) i = Math.min(i, window.WLEDUI.pcPeekMaxIndex(idoc));
        tabHost.classList.toggle('smooth', false);
        tabHost.style.setProperty('--i', i);
        if (typeof iwin.updateTablinks === 'function') iwin.updateTablinks(tabI);
        if (window.WLEDUI.scrollTabIntoView) window.WLEDUI.scrollTabIntoView(idoc, tabI);
        if (peek && !force) return;
      }
      return origOpenTab.call(iwin, tabI, force);
    };
    iwin.openTab._wledUiHost = true;
  }

  if (tabHost._wledUiSwipe) return;
  tabHost._wledUiSwipe = true;

  let x0 = null;
  let scrollS = 0;
  let locked = false;

  function tabCount() {
    return window.WLEDUI.getTabCount(idoc) || 4;
  }

  function currentSlide() {
    const v = tabHost.style.getPropertyValue('--i');
    if (v) return parseInt(v, 10) || 0;
    const n = tabHost.querySelectorAll('.tabcontent').length;
    return n > 0 ? 0 : 0;
  }

  function unify(e) {
    return e.changedTouches ? e.changedTouches[0] : e;
  }

  function hasIroClass(classList) {
    if (!classList) return false;
    for (let i = 0; i < classList.length; i++) {
      if (classList[i].startsWith('Iro')) return true;
    }
    return false;
  }

  function carouselLock(e) {
    if (iwin.simplifiedUI) return;
    if (iwin.pcMode && !window.WLEDUI.carouselMode(idoc)) return;
    const l = e.target.classList;
    const pl = e.target.parentElement && e.target.parentElement.classList;
    if (l.contains('noslide') || hasIroClass(l) || hasIroClass(pl)) return;
    e.stopImmediatePropagation();
    x0 = unify(e).clientX;
    const slides = idoc.getElementsByClassName('tabcontent');
    const iSlide = currentSlide();
    scrollS = slides[iSlide] ? slides[iSlide].scrollTop : 0;
    tabHost.classList.toggle('smooth', !(locked = true));
  }

  function carouselMove(e) {
    if (!locked || iwin.simplifiedUI) return;
    if (iwin.pcMode && !window.WLEDUI.carouselMode(idoc)) return;
    e.stopImmediatePropagation();
    const clientX = unify(e).clientX;
    const wW = iwin.wW || iwin.innerWidth;
    const dx = clientX - x0;
    const s = Math.sign(dx);
    let f = +(s * dx / wW).toFixed(2);
    let iSlide = currentSlide();
    const n = tabCount();
    const slides = idoc.getElementsByClassName('tabcontent');

    if (clientX !== 0 &&
      (iSlide > 0 || s < 0) && (iSlide < n - 1 || s > 0) &&
      f > 0.12 &&
      slides[iSlide] && slides[iSlide].scrollTop === scrollS) {
      iSlide -= s;
      if (window.WLEDUI.pcPeekCarousel(idoc)) {
        iSlide = Math.max(0, Math.min(iSlide, window.WLEDUI.pcPeekMaxIndex(idoc)));
      } else {
        iSlide = Math.max(0, Math.min(iSlide, n - 1));
      }
      tabHost.style.setProperty('--i', iSlide);
      if (typeof iwin.updateTablinks === 'function') iwin.updateTablinks(iSlide);
      f = 1 - f;
    }
    tabHost.style.setProperty('--f', f);
    tabHost.classList.toggle('smooth', !(locked = false));
    x0 = null;
  }

  const cap = { capture: true };
  tabHost.addEventListener('mousedown', carouselLock, cap);
  tabHost.addEventListener('touchstart', carouselLock, cap);
  tabHost.addEventListener('mouseout', carouselMove, cap);
  tabHost.addEventListener('mouseup', carouselMove, cap);
  tabHost.addEventListener('touchend', carouselMove, cap);
}

function mirrorIframeWs(iwin) {
  if (!iwin || iwin._wledUiWsMirror) return;
  iwin._wledUiWsMirror = true;
  function sync() {
    try {
      if (iwin.ws) window.ws = iwin.ws;
    } catch (e) {}
  }
  sync();
  if (typeof iwin.makeWS === 'function' && !iwin.makeWS._wledUiWsMirror) {
    const origMakeWS = iwin.makeWS;
    iwin.makeWS = function () {
      origMakeWS.apply(iwin, arguments);
      sync();
    };
    iwin.makeWS._wledUiWsMirror = true;
  }
}

function schedulePaletteWarmup(iwin) {
  if (!iwin || iwin._wledUiPaletteWarmup) return;
  iwin._wledUiPaletteWarmup = true;

  function refreshPalettes(force) {
    if (typeof iwin.loadPalettes !== 'function' || typeof iwin.loadPalettesData !== 'function') return;
    var stale = !!force;
    if (!stale) {
      if (!iwin.palettesData) stale = true;
      try {
        var raw = localStorage.getItem('wledPalx');
        if (!raw) stale = true;
        else if (iwin.lastinfo) {
          var d = JSON.parse(raw);
          if (!d || !d.p || d.vid !== iwin.lastinfo.vid) stale = true;
        }
      } catch (e) { stale = true; }
    }
    if (!stale) return;

    iwin.palettesData = null;
    iwin.loadPalettes(function () {
      iwin.palettesData = null;
      iwin.loadPalettesData(function () {
        if (typeof iwin.populatePalettes === 'function') iwin.populatePalettes();
        if (typeof iwin.redrawPalPrev === 'function') iwin.redrawPalPrev();
      });
    });
  }

  setTimeout(function () { refreshPalettes(false); }, 3000);
  setTimeout(function () { refreshPalettes(true); }, 8000);
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

function bootstrapIframe(frame, attempt) {
  attempt = attempt || 0;
  let idoc;
  let iwin;
  try {
    idoc = frame.contentDocument;
    iwin = frame.contentWindow;
  } catch (e) {
    idoc = null;
    iwin = null;
  }
  if (!idoc || !iwin || !idoc.getElementById('Colors')) {
    if (attempt < 100) {
      setTimeout(function () { bootstrapIframe(frame, attempt + 1); }, 25);
    }
    return;
  }
  if (!window.WLEDUI || typeof window.WLEDUI._runAll !== 'function') {
    if (attempt < 100) {
      setTimeout(function () { bootstrapIframe(frame, attempt + 1); }, 25);
    }
    return;
  }
  if (!interceptNavigation(iwin) && attempt < 100) {
    setTimeout(function () { bootstrapIframe(frame, attempt + 1); }, 25);
    return;
  }
  if (frame._wledUiBootstrapped) return;
  frame._wledUiBootstrapped = true;
  mirrorTitleAndFavicon(idoc);
  ensureTabLayout(idoc);
  mirrorIframeWs(iwin);
  window.WLEDUI._runAll(idoc);
  if (typeof window.WLEDUI.syncTabLayout === 'function') {
    window.WLEDUI.syncTabLayout(idoc);
  }
  schedulePaletteWarmup(iwin);
  frame._wledUiLoadDone = true;
}

const mainFrame = document.getElementById('main');
patchIframeEarly(mainFrame);

mainFrame.addEventListener('load', function () {
  mainFrame._wledUiLoadDone = false;
  mainFrame._wledUiBootstrapped = false;
  bootstrapIframe(mainFrame, 0);
});

mainFrame.src = '/wled_orig';
)=====";
