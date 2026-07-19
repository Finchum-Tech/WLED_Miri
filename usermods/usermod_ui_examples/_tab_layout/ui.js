// Usermod UI examples support: Tab layout
// This is shared support code, not a visible lettered example.
// Provides common panel markup/styles and asks the host to recalculate the
// carousel or PC multi-column layout after several examples add tabs.
//
// The host already provides syncTabLayout. Individual tab examples also include
// small style fallbacks, so they continue to work when loaded without this file.
//
// Included automatically by -D USERMOD_UI_EXAMPLES.
// See readme.md before reusing these helpers in another usermod.

function uiexEnsurePanelChrome(idoc) {
  if (idoc.getElementById('uiex-panel-chrome-style')) return;

  const style = idoc.createElement('style');
  style.id = 'uiex-panel-chrome-style';
  style.textContent =
    '.uiex-panel{box-sizing:border-box;padding:12px 14px 18px;max-width:100%;}' +
    '.uiex-title{margin:0 0 10px;padding:0;font:600 1.15em/1.3 sans-serif;color:var(--c-f,#eee);}' +
    '.uiex-sep{height:3px;border-radius:2px;background:var(--uiex-accent,#5B7C99);margin:0 0 12px;}' +
    '.uiex-blurb{margin:0 0 12px;font:14px/1.45 sans-serif;color:var(--c-f,#ccc);opacity:.92;}' +
    '.uiex-body{margin:0;}' +
    '.uiex-pill{' +
      'display:block;width:100%;box-sizing:border-box;position:relative;overflow:hidden;' +
      'border-radius:21px;min-height:40px;margin:0 auto 12px;padding:10px 16px 14px;' +
      'border:1px solid var(--c-2,#333);background:var(--c-2,#222);color:var(--c-f,#eee);' +
      'cursor:pointer;font:15px/1.3 sans-serif;text-align:center;' +
    '}' +
    '.uiex-pill:hover{background:var(--c-5,#444);}' +
    '.uiex-pill::after{' +
      'content:"";position:absolute;left:0;right:0;bottom:0;height:4px;' +
      'background:var(--uiex-accent,#5B7C99);' +
    '}';
  idoc.head.appendChild(style);
}

function uiexPanelShell(title, blurb, accent, bodyHtml) {
  return (
    '<div class="uiex-panel" style="--uiex-accent:' + accent + '">' +
      '<h2 class="uiex-title">' + title + '</h2>' +
      '<div class="uiex-sep" aria-hidden="true"></div>' +
      '<p class="uiex-blurb">' + blurb + '</p>' +
      '<div class="uiex-sep" aria-hidden="true"></div>' +
      '<div class="uiex-body">' + bodyHtml + '</div>' +
    '</div>'
  );
}

function uiexLoremIpsumButtons() {
  return (
    '<button type="button" class="uiex-pill" data-uiex-demo="lorem">Lorem</button>' +
    '<button type="button" class="uiex-pill" data-uiex-demo="ipsum">Ipsum</button>'
  );
}

function uiexWireDemoPills(panel, idoc, labelPrefix) {
  const iwin = idoc.defaultView;
  if (!iwin || typeof iwin.showToast !== 'function') return;
  panel.querySelectorAll('.uiex-pill[data-uiex-demo]').forEach(function (btn) {
    btn.addEventListener('click', function () {
      const name = btn.getAttribute('data-uiex-demo') || 'demo';
      var nice = name === 'lorem' ? 'Lorem' : (name === 'ipsum' ? 'Ipsum' : name);
      iwin.showToast((labelPrefix || 'Example') + ': ' + nice + ' button pressed', true);
      iwin.setTimeout(function () {
        const el = idoc.getElementById('toast');
        if (el) el.classList.remove('error', 'show');
      }, 2900);
    });
  });
}

function init(container, idoc) {
  window.WLEDUI = window.WLEDUI || {};
  window.WLEDUI.uiexEnsurePanelChrome = uiexEnsurePanelChrome;
  window.WLEDUI.uiexPanelShell = uiexPanelShell;
  window.WLEDUI.uiexLoremIpsumButtons = uiexLoremIpsumButtons;
  window.WLEDUI.uiexWireDemoPills = uiexWireDemoPills;

  uiexEnsurePanelChrome(idoc);

  if (typeof window.WLEDUI.syncTabLayout === 'function') {
    window.WLEDUI.syncTabLayout(idoc);
  }
}