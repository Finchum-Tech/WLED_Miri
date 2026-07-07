// usermod_ui_examples/new_tab - Recipe C (spec §8.7)

function usermodUiExamplesTabSwitch(idoc, tabI, tabCount) {
  const slide = idoc.querySelector('.container');
  if (!slide) return;
  slide.style.setProperty('--n', tabCount);
  slide.style.setProperty('--i', tabI);
  const links = idoc.getElementsByClassName('tablinks');
  for (let i = 0; i < links.length; i++) {
    links[i].classList.toggle('active', i === tabI);
  }
}

function init(container, idoc) {
  const iwin = idoc.defaultView;
  const tabHost = idoc.querySelector('.container');
  const tabBar = idoc.getElementById('bot');
  if (!tabHost || !tabBar || !iwin) return;
  if (idoc.getElementById('usermod_ui_examples-recipe-c-tab')) return;

  const panel = idoc.createElement('div');
  panel.id = 'usermod_ui_examples-recipe-c-tab';
  panel.className = 'tabcontent';
  panel.innerHTML =
    '<p class="labels hd">Examples</p>' +
    '<p class="helpText">Native <code>tabcontent</code> panel with a bottom-bar button. ' +
    'See <strong>Embed</strong> and <strong>Matrix</strong> tabs for Recipes E and F.</p>';
  tabHost.appendChild(panel);

  const tabIndex = tabBar.querySelectorAll('button.tablinks').length;
  const tabCount = tabIndex + 1;

  if (!iwin.openTab || !iwin.openTab._usermodUiExamplesRecipeC) {
    const original = iwin.openTab;
    if (typeof original === 'function') {
      iwin.openTab = function (tabI, force) {
        if (tabI === tabIndex) {
          usermodUiExamplesTabSwitch(idoc, tabIndex, tabCount);
          iwin.location.hash = 'UIExamplesTab';
          return;
        }
        original.call(iwin, tabI, force);
      };
      iwin.openTab._usermodUiExamplesRecipeC = true;
    }
  }

  const btn = idoc.createElement('button');
  btn.className = 'tablinks';
  btn.innerHTML = '<i class="icons">&#xe88a;</i><p class="tab-label">Examples</p>';
  btn.onclick = function () { iwin.openTab(tabIndex); };
  tabBar.appendChild(btn);
  tabHost.style.setProperty('--n', tabCount);
}
