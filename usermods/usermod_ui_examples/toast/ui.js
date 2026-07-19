// Usermod UI Example B: Toast
// Shows a stock WLED toast once when the main UI loads.
// Demonstrates calling a stock helper through idoc.defaultView, guards against repeated
// initialization, and clears the persistent error-toast style.
//
// Entry point: init(container, idoc). idoc is the live stock WLED document;
// the host may call init again after navigation, so the toast uses a marker.
//
// Load only this example:
//   -D USERMOD_UI  ; enables the main Usermod UI injection host
//   -D USERMOD_UI_INCLUDE='"usermod_ui_examples/toast/ui.js"'  ; bundles only this example, even if enabled usermod folders contain other ui.js files
// In WLED v0.15.x, also keep file://usermods/usermod_ui in lib_deps. See readme.md for complete setup.

function init(container, idoc) {
  if (idoc.getElementById('usermod_ui_examples-toast-fired')) return;

  const marker = idoc.createElement('span');
  marker.id = 'usermod_ui_examples-toast-fired';
  marker.hidden = true;
  idoc.body.appendChild(marker);

  const iwin = idoc.defaultView;
  if (!iwin || typeof iwin.showToast !== 'function') return;

  iwin.showToast('Example B: toast shown on page load', true);
  iwin.setTimeout(function () {
    const el = idoc.getElementById('toast');
    if (el) el.classList.remove('error', 'show');
  }, 2900);
}
