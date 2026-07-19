// Usermod UI Example F: Matrix
// Adds a tab that renders WLED's liveview as a 2D grid or wrapped 1D strip.
// Demonstrates reusing the stock WebSocket, decoding binary liveview frames,
// adapting to device layout, and enabling the stream only while it is visible.
//
// Entry point: init(container, idoc). This example inserts into the live stock
// WLED document and guards its panel, WebSocket hooks, and PC Mode hooks.
//
// WLED serves one liveview client at a time, so close Peek if frames do not arrive.
// Load only this example:
//   -D USERMOD_UI  ; enables the main Usermod UI injection host
//   -D USERMOD_UI_INCLUDE='"usermod_ui_examples/matrix/ui.js"'  ; bundles only this example, even if enabled usermod folders contain other ui.js files
// In WLED v0.15.x, also keep file://usermods/usermod_ui in lib_deps. See readme.md for complete setup.

function init(container, idoc) {
  const tabHost = idoc.querySelector('.container');
  const tabBar = idoc.getElementById('bot');
  const iwin = idoc.defaultView;
  if (!tabHost || !tabBar || !iwin) return;
  if (!idoc.getElementById('Colors')) return;
  if (idoc.getElementById('usermod_ui_examples-matrix-tab')) return;

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
      '.uiex-sep{height:3px;border-radius:2px;background:var(--uiex-accent,#7A6B8A);margin:0 0 12px;}' +
      '.uiex-blurb{margin:0 0 12px;font:14px/1.45 sans-serif;color:var(--c-f,#ccc);opacity:.92;}' +
      '.uiex-body{margin:0;}';
    idoc.head.appendChild(style);
  }

  uiexEnsurePanelChrome(idoc);

  const accent = '#7A6B8A';
  const panel = idoc.createElement('div');
  panel.id = 'usermod_ui_examples-matrix-tab';
  panel.className = 'tabcontent';
  panel.innerHTML =
    '<div class="uiex-panel" style="--uiex-accent:' + accent + '">' +
      '<h2 class="uiex-title">Example F</h2>' +
      '<div class="uiex-sep" aria-hidden="true"></div>' +
      '<p class="uiex-blurb">Live LED preview on the main WLED WebSocket: real device pixels in a native tab, not a mock.</p>' +
      '<div class="uiex-sep" aria-hidden="true"></div>' +
      '<div class="uiex-body">' +
        '<p id="usermod_ui_examples-matrix-help" class="helpText">Liveview grid on the main WebSocket.</p>' +
        '<div id="usermod_ui_examples-matrix-grid" style="margin:12px auto;padding:8px;width:100%;' +
        'max-width:100%;box-sizing:border-box;"></div>' +
        '<p id="usermod_ui_examples-matrix-status" class="helpText" style="text-align:center;">Open this tab for liveview</p>' +
      '</div>' +
    '</div>';
  tabHost.appendChild(panel);

  const grid = panel.querySelector('#usermod_ui_examples-matrix-grid');
  const help = panel.querySelector('#usermod_ui_examples-matrix-help');
  const status = panel.querySelector('#usermod_ui_examples-matrix-status');
  const cells = [];

  let frameCount = 0;
  panel._lvActive = false;
  panel._lvWs = null;
  panel._lvHandler = null;
  panel._mode = '';
  panel._gridW = 0;
  panel._gridH = 0;
  panel._stripCount = 0;
  panel._layoutSource = '';

  function setStatus(text) {
    if (status) status.textContent = text;
  }

  function ledCountFromInfo() {
    if (typeof iwin.ledCount === 'number' && iwin.ledCount > 0) return iwin.ledCount;
    const leds = iwin.lastinfo && iwin.lastinfo.leds;
    return (leds && leds.count) ? leds.count : 0;
  }

  function measurePanelWidth() {
    const w = panel.clientWidth;
    if (w > 0) return w;
    return Math.max(120, (iwin.innerWidth || 320) - 24);
  }

  function layoutFromInfo() {
    const leds = iwin.lastinfo && iwin.lastinfo.leds;
    const mx = leds && leds.matrix;
    if (mx && mx.w > 0 && mx.h > 0) {
      return { mode: '2d', w: mx.w, h: mx.h, source: '2d-config' };
    }
    const count = ledCountFromInfo();
    return { mode: 'strip', count: count > 0 ? count : 25, source: 'strip' };
  }

  function updateHelpText() {
    if (!help) return;
    if (panel._mode === '2d') {
      const n = panel._gridW * panel._gridH;
      help.textContent = 'Liveview ' + panel._gridW + 'x' + panel._gridH + ' matrix (' + n + ' LEDs) on the main WebSocket.';
    } else if (panel._mode === 'strip') {
      help.textContent = 'Liveview strip (' + panel._stripCount + ' LEDs, flex wrap to panel width) on the main WebSocket.';
    } else {
      help.textContent = 'Liveview grid on the main WebSocket.';
    }
  }

  function cellStyle(cellPx) {
    return 'width:' + cellPx + 'px;height:' + cellPx + 'px;background:#111;border-radius:50%;' +
      'box-shadow:0 0 0 1px rgba(255,255,255,.12);flex:0 0 auto;';
  }

  function stripCellSize(count, avail) {
    const gap = 4;
    const minCell = 6;
    const maxCell = 22;
    const perRow = Math.max(1, Math.floor((avail + gap) / (minCell + gap)));
    return Math.max(minCell, Math.min(maxCell, Math.floor((avail - (perRow - 1) * gap) / perRow)));
  }

  function resizeStripCells() {
    if (panel._mode !== 'strip' || cells.length === 0) return;
    const avail = Math.max(40, measurePanelWidth() - 16);
    const cellPx = stripCellSize(cells.length, avail);
    grid.style.gap = '4px';
    const style = cellStyle(cellPx);
    for (let i = 0; i < cells.length; i++) cells[i].style.cssText = style;
  }

  function ensureStrip(count, source) {
    count = Math.max(1, count | 0);
    const rebuild = panel._mode !== 'strip' || panel._stripCount !== count || cells.length !== count;
    panel._mode = 'strip';
    panel._stripCount = count;
    if (source) panel._layoutSource = source;

    grid.style.display = 'flex';
    grid.style.flexWrap = 'wrap';
    grid.style.justifyContent = 'flex-start';
    grid.style.alignContent = 'flex-start';
    grid.style.gridTemplateColumns = '';

    if (rebuild) {
      grid.innerHTML = '';
      cells.length = 0;
      for (let i = 0; i < count; i++) {
        const c = idoc.createElement('div');
        c.title = 'LED ' + i;
        grid.appendChild(c);
        cells.push(c);
      }
    }
    resizeStripCells();
    updateHelpText();
  }

  function ensureGrid(w, h, source) {
    w = Math.max(1, w | 0);
    h = Math.max(1, h | 0);
    const n = w * h;
    const rebuild = panel._mode !== '2d' || panel._gridW !== w || panel._gridH !== h || cells.length !== n;
    panel._mode = '2d';
    panel._gridW = w;
    panel._gridH = h;
    if (source) panel._layoutSource = source;

    const avail = Math.max(40, measurePanelWidth() - 16);
    const cellPx = Math.max(6, Math.min(28, Math.floor(avail / w)));
    grid.style.display = 'grid';
    grid.style.flexWrap = '';
    grid.style.gridTemplateColumns = 'repeat(' + w + ', ' + cellPx + 'px)';
    grid.style.gap = '4px';
    grid.style.width = 'fit-content';
    grid.style.maxWidth = '100%';

    if (rebuild) {
      grid.innerHTML = '';
      cells.length = 0;
      const style = cellStyle(cellPx);
      for (let i = 0; i < n; i++) {
        const c = idoc.createElement('div');
        c.title = 'LED ' + i;
        c.style.cssText = style.replace('flex:0 0 auto;', '');
        grid.appendChild(c);
        cells.push(c);
      }
    } else {
      const style = cellStyle(cellPx).replace('flex:0 0 auto;', '');
      for (let i = 0; i < cells.length; i++) cells[i].style.cssText = style;
    }
    updateHelpText();
  }

  function applyLayout(lay) {
    if (lay.mode === '2d') ensureGrid(lay.w, lay.h, lay.source);
    else ensureStrip(lay.count, lay.source);
  }

  function applyLayoutFromInfo() {
    applyLayout(layoutFromInfo());
  }

  function relayoutToPanel() {
    if (panel._mode === 'strip') resizeStripCells();
    else if (panel._mode === '2d') ensureGrid(panel._gridW, panel._gridH, panel._layoutSource);
  }

  if (typeof ResizeObserver !== 'undefined') {
    const ro = new ResizeObserver(function () { relayoutToPanel(); });
    ro.observe(panel);
  }

  function liveWs() {
    const ws = iwin.ws;
    return ws && ws.readyState === WebSocket.OPEN ? ws : null;
  }

  function layoutLabel() {
    if (panel._mode === 'strip') return panel._stripCount + ' LED strip';
    return panel._gridW + 'x' + panel._gridH;
  }

  function onBinaryFrame(b) {
    if (!panel._lvActive) return;

    if (b[1] === 2) {
      applyLayout({ mode: '2d', w: b[2], h: b[3], source: '2d-live' });
    } else {
      const ledCount = Math.floor((b.length - 2) / 3);
      applyLayout({ mode: 'strip', count: ledCount, source: 'strip' });
    }

    const start = b[1] === 2 ? 4 : 2;
    let anyLit = false;
    const n = Math.min(cells.length, Math.floor((b.length - start) / 3));
    for (let i = 0; i < n; i++) {
      const o = start + i * 3;
      if (o + 2 >= b.length) break;
      if (b[o] || b[o + 1] || b[o + 2]) anyLit = true;
      cells[i].style.background = 'rgb(' + b[o] + ',' + b[o + 1] + ',' + b[o + 2] + ')';
    }

    frameCount++;
    setStatus(
      layoutLabel() + ' liveview - ' + frameCount + ' frame' +
      (frameCount === 1 ? '' : 's') +
      (anyLit ? '' : ' (all black - is power on?)')
    );
  }

  function wsPayloadTag(data) {
    return Object.prototype.toString.call(data);
  }

  function decodeLvPayload(data) {
    const tag = wsPayloadTag(data);
    if (tag === '[object ArrayBuffer]') {
      const b = new Uint8Array(data);
      if (b[0] === 76) onBinaryFrame(b);
      return;
    }
    if (tag === '[object Blob]') {
      data.arrayBuffer().then(function (buf) {
        const b = new Uint8Array(buf);
        if (b[0] === 76) onBinaryFrame(b);
      });
    }
  }

  function handleMessage(ev) {
    decodeLvPayload(ev.data);
  }

  function detachListener() {
    if (panel._lvWs && panel._lvHandler) {
      try { panel._lvWs.removeEventListener('message', panel._lvHandler); } catch (e) {}
    }
    panel._lvWs = null;
    panel._lvHandler = null;
  }

  function attachListener() {
    const ws = liveWs();
    if (!ws) return false;
    if (ws === panel._lvWs && panel._lvHandler) return true;
    detachListener();
    ws.binaryType = 'arraybuffer';
    panel._lvWs = ws;
    panel._lvHandler = handleMessage;
    ws.addEventListener('message', panel._lvHandler);
    return true;
  }

  function installMakeWSHook() {
    if (typeof iwin.makeWS !== 'function' || iwin.makeWS._usermodUiExamplesMatrix) return;
    const origMakeWS = iwin.makeWS;
    iwin.makeWS = function () {
      const prev = iwin.ws;
      origMakeWS.apply(iwin, arguments);
      if (iwin.ws && iwin.ws !== prev) detachListener();
      if (panel._lvActive) {
        attachListener();
        sendLv(true);
      }
    };
    iwin.makeWS._usermodUiExamplesMatrix = true;
  }

  function sendLv(on) {
    const ws = liveWs();
    if (!ws) return false;
    try { ws.send(on ? '{"lv":true}' : '{"lv":false}'); } catch (e) { return false; }
    return true;
  }

  function clearLvTimers() {
    clearTimeout(panel._noFrameTimer);
    clearTimeout(panel._giveUpTimer);
    clearInterval(panel._wsWait);
  }

  function tryActivate() {
    if (!panel._lvActive) return false;
    installMakeWSHook();
    if (!attachListener() || !sendLv(true)) return false;
    return true;
  }

  function pcModeActive() {
    return !!(iwin.pcMode && !iwin.simplifiedUI);
  }

  function enableLiveview() {
    panel._lvActive = true;
    frameCount = 0;
    clearLvTimers();
    applyLayoutFromInfo();
    relayoutToPanel();
    setStatus('Liveview requested...');

    if (!tryActivate()) {
      setStatus('Waiting for main WebSocket...');
      let tries = 0;
      panel._wsWait = setInterval(function () {
        if (!panel._lvActive || frameCount > 0) {
          clearInterval(panel._wsWait);
          return;
        }
        tries++;
        if (tryActivate()) {
          setStatus('Liveview on main WS - waiting for frames...');
          clearInterval(panel._wsWait);
        } else if (tries >= 20) {
          clearInterval(panel._wsWait);
          setStatus('Main WebSocket not available');
        }
      }, 250);
    } else {
      setStatus('Liveview on main WS - waiting for frames...');
    }

    panel._noFrameTimer = setTimeout(function () {
      if (!panel._lvActive || frameCount > 0) return;
      sendLv(true);
      setStatus('No frames yet - retried liveview on main WS');
    }, 2000);

    panel._giveUpTimer = setTimeout(function () {
      if (!panel._lvActive || frameCount > 0) return;
      setStatus('No liveview frames (close Peek if open)');
    }, 8000);
  }

  function disableLiveview() {
    if (!panel._lvActive) return;
    panel._lvActive = false;
    clearLvTimers();
    sendLv(false);
    detachListener();
    setStatus('Open this tab for liveview');
  }

  applyLayoutFromInfo();

  const tabIndex = tabBar.querySelectorAll('button.tablinks').length;
  const btn = idoc.createElement('button');
  btn.className = 'tablinks';
  btn.innerHTML = '<i class="icons">&#xe40a;</i><p class="tab-label">Example F</p>';
  btn.addEventListener('click', function () {
    iwin.openTab(tabIndex);
  });
  tabBar.appendChild(btn);

  if (typeof iwin.openTab === 'function' && !iwin.openTab._usermodUiExamplesMatrix) {
    const origOpenTab = iwin.openTab;
    iwin.openTab = function (tabI, force) {
      if (tabI === tabIndex) enableLiveview();
      else if (!pcModeActive()) disableLiveview();
      return origOpenTab.call(iwin, tabI, force);
    };
    iwin.openTab._usermodUiExamplesMatrix = true;
    iwin.openTab._usermodUiExamplesMatrixTab = tabIndex;
  }

  window.WLEDUI = window.WLEDUI || {};
  window.WLEDUI.matrixLiveviewOff = disableLiveview;

  if (window.WLEDUI && typeof window.WLEDUI.syncTabLayout === 'function') {
    window.WLEDUI.syncTabLayout(idoc);
  }

  setTimeout(function () {
    if (pcModeActive()) enableLiveview();
  }, 0);

  if (typeof iwin.togglePcMode === 'function' && !iwin.togglePcMode._usermodUiExamplesMatrixPc) {
    const origTogglePcMode = iwin.togglePcMode;
    iwin.togglePcMode = function () {
      const wasPc = iwin.pcMode;
      origTogglePcMode.apply(iwin, arguments);
      if (pcModeActive()) enableLiveview();
      else if (wasPc) disableLiveview();
    };
    iwin.togglePcMode._usermodUiExamplesMatrixPc = true;
  }
}
