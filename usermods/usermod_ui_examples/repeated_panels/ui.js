// Usermod UI Example D: Repeated panels
// Creates one tab and panel for each key in a list.
// Demonstrates generating similar interfaces in a loop, deriving per-item IDs,
// wiring each button to its panel, and refreshing the layout after the loop.
//
// Entry point: init(container, idoc). This example inserts into the live stock
// WLED document and checks each generated panel before inserting it again.
//
// Load only this example:
//   -D USERMOD_UI  ; enables the main Usermod UI injection host
//   -D USERMOD_UI_INCLUDE='"usermod_ui_examples/repeated_panels/ui.js"'  ; bundles only this example, even if enabled usermod folders contain other ui.js files
// In WLED v0.15.x, also keep file://usermods/usermod_ui in lib_deps. See readme.md for complete setup.

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
    '.uiex-sep{height:3px;border-radius:2px;background:var(--uiex-accent,#6B8F71);margin:0 0 12px;}' +
    '.uiex-blurb{margin:0 0 12px;font:14px/1.45 sans-serif;color:var(--c-f,#ccc);opacity:.92;}' +
    '.uiex-body{margin:0;}' +
    '.uiex-pill{display:block;width:100%;box-sizing:border-box;position:relative;overflow:hidden;' +
      'border-radius:21px;min-height:40px;margin:0 auto 12px;padding:10px 16px 14px;' +
      'border:1px solid var(--c-2,#333);background:var(--c-2,#222);color:var(--c-f,#eee);' +
      'cursor:pointer;font:15px/1.3 sans-serif;text-align:center;}' +
    '.uiex-pill:hover{background:var(--c-5,#444);}' +
    '.uiex-pill::after{content:"";position:absolute;left:0;right:0;bottom:0;height:4px;background:var(--uiex-accent,#6B8F71);}';
  idoc.head.appendChild(style);
}

function init(container, idoc) {
  if (!idoc.getElementById('Colors')) return;
  const tabHost = idoc.querySelector('.container');
  const tabBar = idoc.getElementById('bot');
  const iwin = idoc.defaultView;
  if (!tabHost || !tabBar || !iwin) return;

  uiexEnsurePanelChrome(idoc);

  const accent = '#6B8F71';
  const blurb =
    'Several similar tabs from one list in a single usermod. Each panel id includes the bus key so bus0, bus1, and bus2 never collide.';

  ['bus0', 'bus1', 'bus2'].forEach(function (bus) {
    const id = 'usermod_ui_examples-' + bus + '-tab';
    if (idoc.getElementById(id)) return;

    const busLabel = bus.replace('bus', 'bus ');
    const title = 'Example D ' + busLabel;
    const body =
      '<button type="button" class="uiex-pill" data-uiex-demo="lorem">Lorem</button>' +
      '<button type="button" class="uiex-pill" data-uiex-demo="ipsum">Ipsum</button>';

    const panel = idoc.createElement('div');
    panel.id = id;
    panel.className = 'tabcontent';
    panel.innerHTML =
      '<div class="uiex-panel" style="--uiex-accent:' + accent + '">' +
        '<h2 class="uiex-title">' + title + '</h2>' +
        '<div class="uiex-sep" aria-hidden="true"></div>' +
        '<p class="uiex-blurb">' + blurb + '</p>' +
        '<div class="uiex-sep" aria-hidden="true"></div>' +
        '<div class="uiex-body">' + body + '</div>' +
      '</div>';
    tabHost.appendChild(panel);

    panel.querySelectorAll('.uiex-pill[data-uiex-demo]').forEach(function (pill) {
      pill.addEventListener('click', function () {
        if (typeof iwin.showToast !== 'function') return;
        var label = pill.getAttribute('data-uiex-demo') === 'lorem' ? 'Lorem' : 'Ipsum';
        iwin.showToast(title + ': ' + label + ' button pressed', true);
        iwin.setTimeout(function () {
          const el = idoc.getElementById('toast');
          if (el) el.classList.remove('error', 'show');
        }, 2900);
      });
    });

    const tabIndex = tabBar.querySelectorAll('button.tablinks').length;
    const btn = idoc.createElement('button');
    btn.className = 'tablinks';
    btn.innerHTML =
      '<i class="icons">&#xe1db;</i><p class="tab-label">Example D ' + busLabel + '</p>';
    btn.addEventListener('click', function () {
      iwin.openTab(tabIndex);
    });
    tabBar.appendChild(btn);
  });

  if (window.WLEDUI && typeof window.WLEDUI.syncTabLayout === 'function') {
    window.WLEDUI.syncTabLayout(idoc);
  }
}