// Usermod UI Example E: Embed
// Adds a tab containing WLED's palette editor in an iframe.
// Demonstrates lazy iframe loading, keeping embedded redirects inside the tab,
// detecting successful palette saves, and refreshing the parent palette data.
//
// Entry point: init(container, idoc). This example inserts into the live stock
// WLED document and guards its panel, listeners, and iframe initialization.
//
// Load only this example:
//   -D USERMOD_UI  ; enables the main Usermod UI injection host
//   -D USERMOD_UI_INCLUDE='"usermod_ui_examples/embed/ui.js"'  ; bundles only this example, even if enabled usermod folders contain other ui.js files
// In WLED v0.15.x, also keep file://usermods/usermod_ui in lib_deps. See readme.md for complete setup.

function usermodUiExamplesInstallEmbedUploadGuard(embedWin) {
  if (!embedWin || embedWin._usermodUiEmbedGuard) return;
  embedWin._usermodUiEmbedGuard = true;

  const XHR = embedWin.XMLHttpRequest;
  const origOpen = XHR.prototype.open;
  const origAdd = XHR.prototype.addEventListener;

  XHR.prototype.open = function (method, url) {
    const path = String(url).replace(/\?.*$/, '');
    this._usermodUiUpload = method === 'POST' && (path === '/upload' || path.endsWith('/upload'));
    return origOpen.apply(this, arguments);
  };

  XHR.prototype.addEventListener = function (type, listener, options) {
    if (type === 'load' && this._usermodUiUpload && typeof listener === 'function') {
      const origListener = listener;
      listener = function (ev) {
        if (this.status >= 200 && this.status < 300) {
          try {
            embedWin.localStorage.removeItem('wledPalx');
          } catch (e) {}
          try {
            embedWin.parent.postMessage({ type: 'usermod_ui_examples-palette-saved' }, '*');
          } catch (e) {}
          if (typeof embedWin.showToast === 'function') {
            embedWin.showToast('Palette saved');
          }
          return;
        }
        return origListener.call(this, ev);
      };
    }
    return origAdd.call(this, type, listener, options);
  };
}

function usermodUiExamplesIsWrapperDoc(doc) {
  return !!(doc && (doc.getElementById('main') || doc.querySelector('iframe#main')));
}

function init(container, idoc) {
  const tabHost = idoc.querySelector('.container');
  const tabBar = idoc.getElementById('bot');
  const iwin = idoc.defaultView;
  if (!tabHost || !tabBar || !iwin) return;
  if (!idoc.getElementById('Colors')) return;
  if (idoc.getElementById('usermod_ui_examples-embed-tab')) return;

  function uiexEnsurePanelChrome(idoc) {
    if (window.WLEDUI && typeof window.WLEDUI.uiexEnsurePanelChrome === 'function') {
      window.WLEDUI.uiexEnsurePanelChrome(idoc);
      return;
    }
    if (idoc.getElementById('uiex-panel-chrome-style')) return;
    const style = idoc.createElement('style');
    style.id = 'uiex-panel-chrome-style';
    style.textContent =
      '.uiex-panel{box-sizing:border-box;padding:12px 14px 18px;max-width:100%;}' +
      '.uiex-title{margin:0 0 10px;padding:0;font:600 1.15em/1.3 sans-serif;color:var(--c-f,#eee);}' +
      '.uiex-sep{height:3px;border-radius:2px;background:var(--uiex-accent,#8B7355);margin:0 0 12px;}' +
      '.uiex-blurb{margin:0 0 12px;font:14px/1.45 sans-serif;color:var(--c-f,#ccc);opacity:.92;}' +
      '.uiex-body{margin:0;}';
    idoc.head.appendChild(style);
  }

  uiexEnsurePanelChrome(idoc);

  const accent = '#8B7355';
  const tab = idoc.createElement('div');
  tab.id = 'usermod_ui_examples-embed-tab';
  tab.className = 'tabcontent';
  tab.innerHTML =
    '<div class="uiex-panel" style="--uiex-accent:' + accent + '">' +
      '<h2 class="uiex-title">Example E</h2>' +
      '<div class="uiex-sep" aria-hidden="true"></div>' +
      '<p class="uiex-blurb">Embeds an existing WLED page in an iframe so you reuse stock UI without rewriting it. Saves stay on this tab.</p>' +
      '<div class="uiex-sep" aria-hidden="true"></div>' +
      '<div class="uiex-body">' +
        '<iframe id="usermod_ui_examples-embed-frame" style="width:100%;height:min(70vh,480px);border:0;border-radius:8px;"></iframe>' +
      '</div>' +
    '</div>';
  tabHost.appendChild(tab);

  const iframe = tab.querySelector('#usermod_ui_examples-embed-frame');
  const cpalUrl = '/cpal.htm';

  if (!iwin._usermodUiExamplesPaletteListener) {
    iwin._usermodUiExamplesPaletteListener = true;
    iwin.addEventListener('message', function (ev) {
      if (!ev.data || ev.data.type !== 'usermod_ui_examples-palette-saved') return;
      if (typeof iwin.loadPalettes !== 'function' || typeof iwin.loadPalettesData !== 'function') return;
      iwin.palettesData = null;
      iwin.loadPalettes(function () {
        iwin.palettesData = null;
        iwin.loadPalettesData(function () {
          if (typeof iwin.populatePalettes === 'function') iwin.populatePalettes();
          if (typeof iwin.redrawPalPrev === 'function') iwin.redrawPalPrev();
        });
      });
    });
  }

  function onEmbedFrameLoad() {
    let ew;
    let edoc;
    try {
      ew = iframe.contentWindow;
      edoc = iframe.contentDocument;
    } catch (e) {
      return;
    }
    if (!ew || !edoc) return;

    if (usermodUiExamplesIsWrapperDoc(edoc) || ew.location.pathname === '/') {
      usermodUiExamplesInstallEmbedUploadGuard(ew);
      iframe.src = cpalUrl;
      return;
    }

    const href = ew.location.href;
    if (href === 'about:blank') {
      usermodUiExamplesInstallEmbedUploadGuard(ew);
      iframe.src = cpalUrl;
      return;
    }

    usermodUiExamplesInstallEmbedUploadGuard(ew);
  }

  function loadEmbedIframe() {
    if (!iframe || iframe.getAttribute('data-loaded')) return;
    iframe.setAttribute('data-loaded', '1');
    iframe.addEventListener('load', onEmbedFrameLoad);
    iframe.src = 'about:blank';
  }

  // Defer the iframe until the panel is on-screen, then wait a beat so PC/peek
  // layout can settle. Tab click uses the same path so behavior stays consistent.
  function scheduleEmbedLoad() {
    if (!iframe || iframe.getAttribute('data-loaded') || iframe._uiexLoadTimer) return;
    iframe._uiexLoadTimer = iwin.setTimeout(function () {
      // Palette editor (cpal.htm) alerts if localStorage wledPalx is missing.
      // On first load the main UI still needs time to regenerate that cache,
      // so the palette editor in particular needs an additional delay here.
      var extraTries = 0;
      function waitForPaletteCacheThenLoad() {
        try {
          if (iwin.localStorage && iwin.localStorage.getItem('wledPalx')) {
            iframe._uiexLoadTimer = null;
            loadEmbedIframe();
            return;
          }
        } catch (e) {}
        extraTries++;
        if (extraTries >= 20) {
          // ~2s extra waited; load anyway (user may still see the alert if cache failed).
          iframe._uiexLoadTimer = null;
          loadEmbedIframe();
          return;
        }
        iframe._uiexLoadTimer = iwin.setTimeout(waitForPaletteCacheThenLoad, 100);
      }
      waitForPaletteCacheThenLoad();
    }, 350);
  }

  function watchEmbedVisibility() {
    if (iframe.getAttribute('data-loaded')) return;
    if (typeof iwin.IntersectionObserver !== 'function') {
      if (iwin.pcMode) scheduleEmbedLoad();
      return;
    }
    if (tab._uiexEmbedObs) return;
    const obs = new iwin.IntersectionObserver(function (entries) {
      for (let i = 0; i < entries.length; i++) {
        if (!entries[i].isIntersecting) continue;
        obs.disconnect();
        tab._uiexEmbedObs = null;
        scheduleEmbedLoad();
        break;
      }
    }, { root: null, threshold: 0.2 });
    tab._uiexEmbedObs = obs;
    obs.observe(tab);
  }

  const tabIndex = tabBar.querySelectorAll('button.tablinks').length;
  const btn = idoc.createElement('button');
  btn.className = 'tablinks';
  btn.innerHTML = '<i class="icons">&#xe3b3;</i><p class="tab-label">Example E</p>';
  btn.addEventListener('click', function () {
    iwin.openTab(tabIndex);
    scheduleEmbedLoad();
  });
  tabBar.appendChild(btn);

  if (window.WLEDUI && typeof window.WLEDUI.syncTabLayout === 'function') {
    window.WLEDUI.syncTabLayout(idoc);
  }

  watchEmbedVisibility();

  if (typeof iwin.togglePcMode === 'function' && !iwin.togglePcMode._usermodUiExamplesEmbedPc) {
    const origTogglePcMode = iwin.togglePcMode;
    iwin.togglePcMode = function () {
      const wasPc = iwin.pcMode;
      origTogglePcMode.apply(iwin, arguments);
      if (iwin.pcMode && !wasPc) watchEmbedVisibility();
    };
    iwin.togglePcMode._usermodUiExamplesEmbedPc = true;
  }
}
