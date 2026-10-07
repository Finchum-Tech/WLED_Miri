// Miri Core UI host for Usermod UI injection.
// Inserts #miri-panels into the live stock WLED document and loads Core's
// brand CSS + panel loader so sub-mod /miri/panel/* routes can populate.
//
// Requires: -D USERMOD_UI and custom_usermods including usermod_ui + miri

function init(container, idoc) {
  if (idoc.getElementById('miri-panels')) return;

  const anchor =
    idoc.getElementById('info') ||
    idoc.querySelector('.container') ||
    idoc.body;
  if (!anchor) return;

  if (!idoc.querySelector('link[href="/miri-brand.css"]')) {
    const link = idoc.createElement('link');
    link.rel = 'stylesheet';
    link.href = '/miri-brand.css';
    idoc.head.appendChild(link);
  }

  const panels = idoc.createElement('div');
  panels.id = 'miri-panels';
  panels.setAttribute('data-miri', 'panels');
  anchor.insertAdjacentElement('beforeend', panels);

  if (!idoc.querySelector('script[src="/miri-ui.js"]')) {
    const script = idoc.createElement('script');
    script.src = '/miri-ui.js';
    script.defer = true;
    idoc.body.appendChild(script);
  }

  const iwin = idoc.defaultView;
  if (iwin && typeof iwin.size === 'function') {
    iwin.size();
  }
}
