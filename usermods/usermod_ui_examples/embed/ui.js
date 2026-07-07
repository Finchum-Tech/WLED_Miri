// usermod_ui_examples/embed - Recipe E (spec section 8.7)

function init(container, idoc) {
  const tabHost = idoc.querySelector('.container');
  const tabBar = idoc.getElementById('bot');
  const iwin = idoc.defaultView;
  if (!tabHost || !tabBar || !iwin) return;
  if (idoc.getElementById('usermod_ui_examples-embed-tab')) return;

  const tab = idoc.createElement('div');
  tab.id = 'usermod_ui_examples-embed-tab';
  tab.className = 'tabcontent';
  tab.innerHTML =
    '<p class="labels hd">Embed</p>' +
    '<p class="helpText">Palette editor via iframe. Open after the main page has loaded once ' +
    '(needed to rebuild palette cache in localStorage after clearing site data).</p>' +
    '<iframe data-src="/cpal.htm" style="width:100%;height:min(70vh,480px);border:0;border-radius:8px;"></iframe>';
  tabHost.appendChild(tab);

  const iframe = tab.querySelector('iframe');

  function loadEmbedIframe() {
    if (!iframe || iframe.src) return;
    iframe.src = iframe.getAttribute('data-src') || '/cpal.htm';
  }

  const tabIndex = tabBar.querySelectorAll('button.tablinks').length;
  const tabCount = tabIndex + 1;
  const btn = idoc.createElement('button');
  btn.className = 'tablinks';
  btn.innerHTML = '<i class="icons">&#xe3b3;</i><p class="tab-label">Embed</p>';
  btn.addEventListener('click', function () {
    iwin.openTab(tabIndex);
    loadEmbedIframe();
  });
  tabBar.appendChild(btn);
  tabHost.style.setProperty('--n', tabCount);
}
