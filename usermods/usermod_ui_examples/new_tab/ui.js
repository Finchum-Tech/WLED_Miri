// Usermod UI Example C: New tab
// Adds one bottom-bar button and one matching panel.
// Demonstrates WLED's tabcontent/tablinks pattern, calculating a tab index,
// opening the panel with openTab, and refreshing the tab layout.
//
// Entry point: init(container, idoc). This example inserts into the live stock
// WLED document, so it checks for its panel before inserting it again.
//
// Load only this example:
//   -D USERMOD_UI  ; enables the main Usermod UI injection host
//   -D USERMOD_UI_INCLUDE='"usermod_ui_examples/new_tab/ui.js"'  ; bundles only this example, even if enabled usermod folders contain other ui.js files
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
    '.uiex-sep{height:3px;border-radius:2px;background:var(--uiex-accent,#5B7C99);margin:0 0 12px;}' +
    '.uiex-blurb{margin:0 0 12px;font:14px/1.45 sans-serif;color:var(--c-f,#ccc);opacity:.92;}' +
    '.uiex-body{margin:0;}' +
    '.uiex-pill{display:block;width:100%;box-sizing:border-box;position:relative;overflow:hidden;' +
      'border-radius:21px;min-height:40px;margin:0 auto 12px;padding:10px 16px 14px;' +
      'border:1px solid var(--c-2,#333);background:var(--c-2,#222);color:var(--c-f,#eee);' +
      'cursor:pointer;font:15px/1.3 sans-serif;text-align:center;}' +
    '.uiex-pill:hover{background:var(--c-5,#444);}' +
    '.uiex-pill::after{content:"";position:absolute;left:0;right:0;bottom:0;height:4px;background:var(--uiex-accent,#5B7C99);}';
  idoc.head.appendChild(style);
}

function init(container, idoc) {
  const iwin = idoc.defaultView;
  const tabHost = idoc.querySelector('.container');
  const tabBar = idoc.getElementById('bot');
  if (!tabHost || !tabBar || !iwin) return;
  if (!idoc.getElementById('Colors')) return;
  if (idoc.getElementById('usermod_ui_examples-example-c-tab')) return;

  uiexEnsurePanelChrome(idoc);

  const accent = '#5B7C99';
  const title = 'Example C';
  const blurb =
    'One new bottom-bar tab with a single panel. Use this when your usermod adds one feature screen of its own.';
  const body =
    '<button type="button" class="uiex-pill" data-uiex-demo="lorem">Lorem ipsum dolor</button>' +
    '<button type="button" class="uiex-pill" data-uiex-demo="ipsum">sit amet consectetur</button>';

  const panel = idoc.createElement('div');
  panel.id = 'usermod_ui_examples-example-c-tab';
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

  panel.querySelectorAll('.uiex-pill[data-uiex-demo]').forEach(function (btn) {
    btn.addEventListener('click', function () {
      if (typeof iwin.showToast !== 'function') return;
      var label = btn.textContent.trim();
      iwin.showToast('Example C: ' + label, true);
      iwin.setTimeout(function () {
        const el = idoc.getElementById('toast');
        if (el) el.classList.remove('error', 'show');
      }, 2900);
    });
  });

  const tabIndex = tabBar.querySelectorAll('button.tablinks').length;

  if (!iwin.openTab || !iwin.openTab._usermodUiExamplesExampleC) {
    const original = iwin.openTab;
    if (typeof original === 'function') {
      iwin.openTab = function (tabI, force) {
        if (tabI === tabIndex) {
          if (window.WLEDUI && window.WLEDUI.pcShowsAllColumns &&
              !window.WLEDUI.pcShowsAllColumns(idoc) &&
              typeof window.WLEDUI.matrixLiveviewOff === 'function') {
            window.WLEDUI.matrixLiveviewOff();
          }
          iwin.location.hash = 'UIExamplesTab';
          return original.call(iwin, tabIndex, force);
        }
        return original.call(iwin, tabI, force);
      };
      iwin.openTab._usermodUiExamplesExampleC = true;
    }
  }

  const btn = idoc.createElement('button');
  btn.className = 'tablinks';
  btn.innerHTML = '<i class="icons">&#xe88a;</i><p class="tab-label">Example C</p>';
  btn.onclick = function () { iwin.openTab(tabIndex); };
  tabBar.appendChild(btn);

  if (window.WLEDUI && typeof window.WLEDUI.syncTabLayout === 'function') {
    window.WLEDUI.syncTabLayout(idoc);
  }
}