// Usermod UI Example G: Button Toast
// Adds a stock-looking button to WLED's top row and shows a toast when clicked.
// Demonstrates anchoring beside existing UI, copying stock button markup,
// wiring an action, and calling showToast through idoc.defaultView.
//
// Entry point: init(container, idoc). idoc is the live stock WLED document;
// the host may call init again after navigation, so the button is guarded.
// buttonPcm is only the insertion anchor; this example does not use PC Mode.
//
// Load only this example:
//   -D USERMOD_UI  ; enables the main Usermod UI injection host
//   -D USERMOD_UI_INCLUDE='"usermod_ui_examples/button_toast/ui.js"'  ; bundles only this example, even if enabled usermod folders contain other ui.js files
// In WLED v0.15.x, also keep file://usermods/usermod_ui in lib_deps. See readme.md for complete setup.

function init(container, idoc) {
  // Guard: init can run again on navigation - do not insert a second button.
  if (idoc.getElementById('button_toast-btn')) return;

  var anchorButton = idoc.getElementById('buttonPcm');
  var iwin = idoc.defaultView;
  if (!anchorButton || !iwin) return;

  var btn = idoc.createElement('button');
  btn.id = 'button_toast-btn';
  btn.type = 'button';
  btn.title = 'Show a toast notification';
  // Match WLED top-bar button markup so it looks native.
  btn.innerHTML =
    '<i class="icons">&#xe409;</i><p class="tab-label">Toast</p>';

  btn.addEventListener('click', function () {
    if (typeof iwin.showToast !== 'function') return;
    iwin.showToast('Example G: toast from the top-bar button', true);
    iwin.setTimeout(function () {
      var el = idoc.getElementById('toast');
      if (el) el.classList.remove('error', 'show');
    }, 2900);
  });

  // Add the example to WLED's stock top button row.
  anchorButton.insertAdjacentElement('afterend', btn);
}