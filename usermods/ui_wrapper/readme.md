# UI Wrapper usermod

Iframe host for the stock WLED control UI. Other usermods extend the main page by adding a `ui.js` file — no edits to `index.htm` or `PAGE_index`.

Full architecture: `wled-ui-wrapper-usermod-spec.md` (v0.3). Author guide: spec Appendix B.

**Target:** WLED 0.15.x (developed on 0.15.1.beta2; validate against 0.15.4+ before upstream PR).

## Enable (0.15.x)

```ini
[env:esp32dev_myconfig]
extends = env:esp32dev
build_flags = ${common.build_flags} ${esp32.build_flags}
  -D USERMOD_UI_WRAPPER
lib_deps = ${esp32.lib_deps}
  file://usermods/ui_wrapper
```

Optional demo UI (`usermods/ui_wrapper_demo/ui.js`):

```ini
  -D USERMOD_UI_WRAPPER_DEMO
```

Or use the bundled environment:

```ini
[platformio]
default_envs = esp32dev_ui_wrapper
```

On 0.15.x you still need `-D USERMOD_UI_WRAPPER` so `usermods_list.cpp` registers the host. The `file://` line pulls in `library.json` so `tools/bundle_ui.py` runs at build time.

## Routes

| Route | Purpose |
|---|---|
| `GET /` | Wrapper page (iframe + bundle + injector) |
| `GET /wled_orig` | Stock main UI (`serveOriginalMainUI`) |
| `GET /usermod_inject.js` | Title/favicon mirror, `getURL()` fix, `_runAll` dispatch |
| `GET /usermod_ui_bundle.js` | Pre-gzipped compile-time UI bundle |

## Add UI to your usermod

1. Enable this wrapper (above) and your usermod with `-D USERMOD_YOUR_NAME`.
2. Add `usermods/your_name/ui.js` with an `init(container, idoc)` function.
3. Rebuild — the bundler discovers your mod via `USERMOD_*` flags and includes `ui.js` automatically.

```js
function init(container, idoc) {
  container.innerHTML = '<div class="yourmod-panel">Hello</div>';
}
```

Prefix any `id` you create in `idoc` (WLED's document) with your usermod name. Work inside `container` whenever possible.

## Core dependency

`serveOriginalMainUI()` in `wled_server.cpp` replicates stock `/` handler logic. This is the only non-usermod hook.

## 16.0+ (later)

Replace `file://` with `custom_usermods = ui_wrapper` once targeting WLED 16 — see spec §8.4.
