// usermod_ui_examples/demo_bar - Recipe A (spec sections 8.7 and 8.8; read this first)

function init(container, idoc) {
  if (idoc.getElementById('demo_bar-strip')) return;

  const top = idoc.getElementById('top');
  if (!top) return;

  const bar = idoc.createElement('div');
  bar.id = 'demo_bar-strip';
  bar.innerHTML =
    '<span class="demo_bar-label">Example A</span>' +
    '<span class="demo_bar-value">Usermod UI active</span>';
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
