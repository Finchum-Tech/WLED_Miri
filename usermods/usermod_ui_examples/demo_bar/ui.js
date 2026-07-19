// Usermod UI Example A: Demo bar
// Adds a status strip below WLED's top button row.
// Demonstrates guarded insertion into idoc, scoped styling, and calling size()
// after changing the header layout.
//
// Entry point: init(container, idoc). idoc is the live stock WLED document;
// the host may call init again after navigation, so inserted nodes are guarded.
//
// Load only this example:
//   -D USERMOD_UI  ; enables the main Usermod UI injection host
//   -D USERMOD_UI_INCLUDE='"usermod_ui_examples/demo_bar/ui.js"'  ; bundles only this example, even if enabled usermod folders contain other ui.js files
// In WLED v0.15.x, also keep file://usermods/usermod_ui in lib_deps. See readme.md for complete setup.

function init(container, idoc) {
  if (idoc.getElementById('demo_bar-strip')) return;

  const top = idoc.getElementById('top');
  if (!top) return;

  const bar = idoc.createElement('div');
  bar.id = 'demo_bar-strip';
  bar.innerHTML =
    '<span class="demo_bar-label">Example A</span>' +
    '<span class="demo_bar-value">Usermod UI static bar active</span>';
  bar.style.cssText =
    'clear:both;display:block;width:100%;box-sizing:border-box;padding:8px 12px;text-align:center;' +
    'background:#08131f;color:#7fd4ff;font:13px/1.45 sans-serif;border-bottom:1px solid #1e3a5f;';

  if (!idoc.getElementById('demo_bar-strip-style')) {
    const style = idoc.createElement('style');
    style.id = 'demo_bar-strip-style';
    style.textContent =
      '#demo_bar-strip .demo_bar-label{display:block;font-weight:600;margin-bottom:2px;}' +
      '#demo_bar-strip .demo_bar-value{opacity:.9;font-size:0.92em;}';
    idoc.head.appendChild(style);
  }

  const tabTop = top.querySelector('.tab.top');
  (tabTop || top).insertAdjacentElement('afterend', bar);

  const iwin = idoc.defaultView;
  if (iwin && typeof iwin.size === 'function') {
    iwin.size();
    iwin.addEventListener('resize', function onResize() {
      iwin.size();
    });
  }
}
