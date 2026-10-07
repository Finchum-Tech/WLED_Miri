#pragma once

static const char MIRI_SPHERE_HTML[] PROGMEM = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
  <title>Color Sphere</title>
  <style>
    :root { color-scheme: dark; }
    body {
      margin: 0;
      font-family: Arial, sans-serif;
      background: #111;
      color: #e8e8e8;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      min-height: 100vh;
      gap: 14px;
      touch-action: none;
    }
    #sphere {
      width: min(82vw, 320px);
      height: min(82vw, 320px);
      border-radius: 50%;
      box-shadow: 0 0 20px rgba(0, 0, 0, 0.6);
      background: #222;
      display: block;
      touch-action: none;
      cursor: crosshair;
    }
    #readout {
      display: flex;
      align-items: center;
      gap: 12px;
      background: #1d1d1d;
      border: 1px solid #333;
      border-radius: 999px;
      padding: 8px 12px;
      font-size: 14px;
      letter-spacing: 0.03em;
    }
    #swatch {
      width: 20px;
      height: 20px;
      border-radius: 50%;
      border: 1px solid #666;
      box-shadow: inset 0 0 0 1px rgba(0, 0, 0, 0.35);
    }
    #status { color: #8fd18f; font-size: 12px; }
  </style>
</head>
<body>
  <canvas id="sphere" width="320" height="320" aria-label="Color sphere"></canvas>
  <div id="readout">
    <div id="swatch"></div>
    <span id="rgb">R:0 G:0 B:0</span>
    <span id="status">ready</span>
  </div>

  <script>
    const canvas = document.getElementById('sphere');
    const ctx = canvas.getContext('2d');
    const swatch = document.getElementById('swatch');
    const rgbOut = document.getElementById('rgb');
    const statusOut = document.getElementById('status');

    const r = canvas.width / 2;
    const cx = r;
    const cy = r;
    let pointer = { x: cx, y: cy };
    let lastSent = 0;
    let pending = null;
    const SEND_MS = 90;

    function hsvToRgb(h, s, v) {
      const c = v * s;
      const hp = h / 60;
      const x = c * (1 - Math.abs((hp % 2) - 1));
      let rr = 0, gg = 0, bb = 0;
      if (hp >= 0 && hp < 1)      { rr = c; gg = x; bb = 0; }
      else if (hp < 2)            { rr = x; gg = c; bb = 0; }
      else if (hp < 3)            { rr = 0; gg = c; bb = x; }
      else if (hp < 4)            { rr = 0; gg = x; bb = c; }
      else if (hp < 5)            { rr = x; gg = 0; bb = c; }
      else                        { rr = c; gg = 0; bb = x; }
      const m = v - c;
      return [
        Math.round((rr + m) * 255),
        Math.round((gg + m) * 255),
        Math.round((bb + m) * 255)
      ];
    }

    function pointToRgb(x, y) {
      const dx = (x - cx) / r;
      const dy = (y - cy) / r;
      const d2 = dx * dx + dy * dy;
      const sat = Math.min(1, Math.sqrt(d2));
      const z = Math.sqrt(Math.max(0, 1 - Math.min(1, d2)));
      const hue = (Math.atan2(dy, dx) * 180 / Math.PI + 360) % 360;
      const value = 0.2 + 0.8 * z;
      return hsvToRgb(hue, sat, value);
    }

    function drawSphere() {
      const img = ctx.createImageData(canvas.width, canvas.height);
      for (let y = 0; y < canvas.height; y++) {
        for (let x = 0; x < canvas.width; x++) {
          const dx = (x - cx) / r;
          const dy = (y - cy) / r;
          const d2 = dx * dx + dy * dy;
          const idx = (y * canvas.width + x) * 4;
          if (d2 > 1) {
            img.data[idx + 3] = 0;
            continue;
          }
          const [rr, gg, bb] = pointToRgb(x, y);
          img.data[idx] = rr;
          img.data[idx + 1] = gg;
          img.data[idx + 2] = bb;
          img.data[idx + 3] = 255;
        }
      }
      ctx.putImageData(img, 0, 0);
      ctx.fillStyle = 'rgba(255,255,255,0.7)';
      ctx.beginPath();
      ctx.arc(pointer.x, pointer.y, 5, 0, Math.PI * 2);
      ctx.fill();
      ctx.strokeStyle = 'rgba(0,0,0,0.7)';
      ctx.lineWidth = 2;
      ctx.stroke();
    }

    function sendColor(rgb) {
      const payload = {"seg":[{"id":0,"col":[[rgb[0], rgb[1], rgb[2]]]}]};
      fetch('/json/state', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      }).then(() => {
        statusOut.textContent = 'sending';
        statusOut.style.color = '#8fd18f';
      }).catch(() => {
        statusOut.textContent = 'offline';
        statusOut.style.color = '#d68e8e';
      });
    }

    function queueSend(rgb) {
      const now = performance.now();
      if (now - lastSent >= SEND_MS) {
        lastSent = now;
        pending = null;
        sendColor(rgb);
      } else {
        pending = rgb;
      }
    }

    setInterval(() => {
      if (pending && performance.now() - lastSent >= SEND_MS) {
        const rgb = pending;
        pending = null;
        lastSent = performance.now();
        sendColor(rgb);
      }
    }, SEND_MS);

    function clampToSphere(x, y) {
      const dx = x - cx;
      const dy = y - cy;
      const dist = Math.hypot(dx, dy);
      if (dist <= r) return { x, y };
      const scale = r / dist;
      return { x: cx + dx * scale, y: cy + dy * scale };
    }

    function updatePointer(x, y) {
      pointer = clampToSphere(x, y);
      const rgb = pointToRgb(pointer.x, pointer.y);
      swatch.style.backgroundColor = `rgb(${rgb[0]},${rgb[1]},${rgb[2]})`;
      rgbOut.textContent = `R:${rgb[0]} G:${rgb[1]} B:${rgb[2]}`;
      drawSphere();
      queueSend(rgb);
    }

    function eventPos(evt) {
      const rect = canvas.getBoundingClientRect();
      const t = evt.touches && evt.touches.length ? evt.touches[0] : evt;
      const sx = canvas.width / rect.width;
      const sy = canvas.height / rect.height;
      return { x: (t.clientX - rect.left) * sx, y: (t.clientY - rect.top) * sy };
    }

    let dragging = false;
    canvas.addEventListener('pointerdown', (evt) => {
      dragging = true;
      canvas.setPointerCapture(evt.pointerId);
      const p = eventPos(evt);
      updatePointer(p.x, p.y);
    });
    canvas.addEventListener('pointermove', (evt) => {
      if (!dragging) return;
      const p = eventPos(evt);
      updatePointer(p.x, p.y);
    });
    canvas.addEventListener('pointerup', () => { dragging = false; });
    canvas.addEventListener('pointercancel', () => { dragging = false; });

    drawSphere();
    updatePointer(cx, cy);
  </script>
</body>
</html>
)html";