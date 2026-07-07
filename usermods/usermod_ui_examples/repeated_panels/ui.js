// usermod_ui_examples/repeated_panels - Recipe D (spec §8.7)

function init(container, idoc) {
  const tabHost = idoc.querySelector('.container');
  if (!tabHost) return;

  ['bus0', 'bus1', 'bus2'].forEach(function (bus) {
    const id = 'usermod_ui_examples-' + bus + '-tab';
    if (idoc.getElementById(id)) return;
    const tab = idoc.createElement('div');
    tab.id = id;
    tab.className = 'tabcontent';
    tab.innerHTML =
      '<p class="labels hd">' + bus + '</p>' +
      '<p class="helpText">Repeated panels; each id is prefixed and unique per item.</p>';
    tabHost.appendChild(tab);
  });
}
