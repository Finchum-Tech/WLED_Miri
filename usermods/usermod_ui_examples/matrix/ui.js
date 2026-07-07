// usermod_ui_examples/matrix - Recipe F (spec §8.7): tab + liveview WebSocket 5x5 grid

function init(container, idoc) {
  const tabHost = idoc.querySelector('.container');
  const tabBar = idoc.getElementById('bot');
  const iwin = idoc.defaultView;
  if (!tabHost || !tabBar || !iwin) return;
  if (idoc.getElementById('usermod_ui_examples-matrix-tab')) return;

  const panel = idoc.createElement('div');
  panel.id = 'usermod_ui_examples-matrix-tab';
  panel.className = 'tabcontent';
  panel.innerHTML =
    '<p class="labels hd">Matrix</p>' +
    '<p class="helpText">First 25 LEDs via liveview WebSocket. Change colors on other tabs and watch cells update.</p>' +
    '<div id="usermod_ui_examples-matrix-grid" style="display:grid;grid-template-columns:repeat(5,1fr);' +
    'gap:6px;max-width:200px;margin:12px auto;padding:8px;"></div>' +
    '<p id="usermod_ui_examples-matrix-status" class="helpText" style="text-align:center;">Connecting...</p>';
  tabHost.appendChild(panel);

  const grid = panel.querySelector('#usermod_ui_examples-matrix-grid');
  const status = panel.querySelector('#usermod_ui_examples-matrix-status');
  const cells = [];
  for (let i = 0; i < 25; i++) {
    const c = idoc.createElement('div');
    c.style.cssText =
      'width:100%;aspect-ratio:1;background:#111;border-radius:50%;' +
      'box-shadow:0 0 0 2px rgba(255,255,255,.12);';
    c.title = 'LED ' + i;
    grid.appendChild(c);
    cells.push(c);
  }

  const tabIndex = tabBar.querySelectorAll('button.tablinks').length;
  const tabCount = tabIndex + 1;
  const btn = idoc.createElement('button');
  btn.className = 'tablinks';
  btn.innerHTML = '<i class="icons">&#xe40a;</i><p class="tab-label">Matrix</p>';
  btn.addEventListener('click', function () { iwin.openTab(tabIndex); });
  tabBar.appendChild(btn);
  tabHost.style.setProperty('--n', tabCount);

  const proto = iwin.location.protocol === 'https:' ? 'wss' : 'ws';
  const ws = new iwin.WebSocket(proto + '://' + iwin.location.host + '/ws');
  ws.binaryType = 'arraybuffer';
  ws.addEventListener('open', function () {
    ws.send('{"lv":true}');
    if (status) status.textContent = 'Liveview connected';
  });
  ws.addEventListener('message', function (ev) {
    if (!(ev.data instanceof ArrayBuffer)) return;
    const b = new Uint8Array(ev.data);
    if (b[0] !== 76) return;
    for (let i = 0; i < 25; i++) {
      const o = 2 + i * 3;
      if (o + 2 >= b.length) break;
      cells[i].style.background = 'rgb(' + b[o] + ',' + b[o + 1] + ',' + b[o + 2] + ')';
    }
  });
  ws.addEventListener('close', function () {
    if (status) status.textContent = 'Liveview disconnected';
  });
  panel._usermodUiExamplesWs = ws;
}
