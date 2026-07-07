// usermod_ui_examples/toast - Recipe B (spec section 8.7)

function init(container, idoc) {
  if (idoc.getElementById('usermod_ui_examples-toast-fired')) return;

  const marker = idoc.createElement('span');
  marker.id = 'usermod_ui_examples-toast-fired';
  marker.hidden = true;
  idoc.body.appendChild(marker);

  const iwin = idoc.defaultView;
  if (iwin && typeof iwin.showToast === 'function') {
    iwin.showToast('Example B - Toast (usermod_ui_examples)');
  }
}
