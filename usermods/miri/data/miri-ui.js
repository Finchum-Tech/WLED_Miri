(() => {
  const panelRoot = document.getElementById("miri-panels");
  if (!panelRoot) return;

  const panelRoutes = ["tempsensor", "fusemonitor", "pwm", "iicie", "colorsphere"];

  async function loadPanel(name) {
    try {
      const response = await fetch(`/miri/panel/${name}`, { cache: "no-store" });
      if (!response.ok) return;
      const html = await response.text();
      if (!html.trim()) return;
      panelRoot.insertAdjacentHTML("beforeend", html);
    } catch (_error) {
      // Missing modules are optional.
    }
  }

  panelRoutes.reduce((chain, name) => chain.then(() => loadPanel(name)), Promise.resolve());
})();
