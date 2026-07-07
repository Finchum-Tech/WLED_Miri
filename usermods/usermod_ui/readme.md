# usermod_ui

Iframe host for the stock WLED control UI. Other usermods extend the main page by adding a `ui.js` file — no edits to `index.htm` or `PAGE_index`.

Full architecture: `wled-ui-wrapper-usermod-spec.md` (v0.3). Author guide: spec Appendix B.

**Target:** WLED 0.15.x (developed on 0.15.1.beta2; validate against 0.15.4+ before upstream PR).

---

## Porting checklist (read this when cherry-picking to another WLED branch)

These are small, easy-to-miss edits. **Every item must be checked** when moving Usermod UI to a different fork or WLED version. Values in *italics* are for **this** tree as of the initial commit — yours may differ.

| # | File | What to verify |
|---|------|----------------|
| 1 | `wled00/const.h` | Pick an **unused** `USERMOD_ID_*` number. Here: `USERMOD_ID_UI` = **55**. Do not collide with IDs already taken on the target branch. |
| 2 | `wled00/usermods_list.cpp` (**include block**, ~top) | `#ifdef USERMOD_UI` → `#include "../usermods/usermod_ui/usermod_ui.h"` |
| 3 | `wled00/usermods_list.cpp` (**register block**, ~bottom) | `#ifdef USERMOD_UI` → `UsermodManager::add(new UsermodUI());` — **both** places, not just one. |
| 4 | `wled00/fcn_declare.h` | `void serveOriginalMainUI(AsyncWebServerRequest *request);` |
| 5 | `wled00/wled_server.cpp` | `serveOriginalMainUI()` implementation (stock `/` handler logic) — the **only** non-usermod core hook. |
| 6 | `usermods/usermod_ui/usermod_ui.h` | `getId()` returns the same ID as `const.h`. `PAGE_index` length symbol is **`PAGE_index_L`**, not `PAGE_index_length`. |
| 7 | `platformio.ini` or override | `-D USERMOD_UI` **and** `lib_deps = … file://usermods/usermod_ui` (both required on 0.15.x). |
| 8 | Bundler discovery | `USERMOD_FOO` → recurse `usermods/foo/` or `usermods/usermod_foo/` for every `ui.js` (skip `node_modules/`, `vendor/`, `third_party/`, dot-dirs). Multi-file keys: `foo__subdir`. Escape hatch: `-D USERMOD_UI_INCLUDE='"foo/panel/ui.js"'` |
| 9 | Demo mod | **Bundler-only** — `USERMOD_UI_EXAMPLES` recurses `usermods/usermod_ui_examples/*/ui.js` (one file per recipe + `demo_bar`). No C++ class / `UsermodManager::add` / `USERMOD_ID_*`. |
| 10 | Matrix recipe | `usermod_ui_examples/matrix/ui.js` uses liveview WebSocket + dynamic `openTab` index from `#bot` button count — re-verify if stock tab count changes. |

After porting: build `esp32dev_usermod_ui`, flash, open `/`, confirm the **Usermod UI active** bar sits below the top buttons, open **Examples** / **Matrix** / **Embed** tabs, and watch the 5×5 grid update when you change colors.

---

## Enable (0.15.x)

```ini
[env:esp32dev_myconfig]
extends = env:esp32dev
build_flags = ${common.build_flags} ${esp32.build_flags}
  -D USERMOD_UI
lib_deps = ${esp32.lib_deps}
  file://usermods/usermod_ui
```

Optional demo (all recipes + demo bar — see `usermods/usermod_ui_examples/readme.md`):

```ini
  -D USERMOD_UI_EXAMPLES
```

**`usermod_ui_examples`** — one `ui.js` per recipe (`demo_bar` = Recipe A, `toast`, `new_tab`, `repeated_panels`, `embed`, `matrix`). Start with `demo_bar/ui.js`.

Or copy `usermods/usermod_ui/platformio_override.sample.ini` to project root as `platformio_override.ini`.

Or use the bundled environment:

```ini
[platformio]
default_envs = esp32dev_usermod_ui
```

The `esp32dev_usermod_ui` env uses `library.json` → `tools/bundle_entry.py`, which always runs the repo-source `bundle_ui.py`. After editing any `ui.js`, rebuild (or run `python usermods/usermod_ui/tools/regen_bundle_dev.py` for a quick regen without a full compile).

On 0.15.x you still need `-D USERMOD_UI` so `usermods_list.cpp` registers the host. The `file://` line pulls in `library.json` so `tools/bundle_entry.py` runs the bundler from source at build time (avoids stale copies under `.pio/libdeps/`).

## Routes

| Route | Purpose |
|---|---|
| `GET /` | Wrapper page (iframe + bundle + injector) |
| `GET /wled_orig` | Stock main UI (`serveOriginalMainUI`) |
| `GET /usermod_inject.js` | Title/favicon mirror, `getURL()` fix, `_runAll` dispatch |
| `GET /usermod_ui_bundle.js` | Pre-gzipped compile-time UI bundle |

## Add UI to your usermod

1. Enable this framework (above) and your usermod with `-D USERMOD_YOUR_NAME`.
2. Add `usermods/your_name/ui.js` (or multiple `ui.js` files in subfolders) with an `init(container, idoc)` function.
3. Rebuild — the bundler recurses your enabled usermod folder and bundles every `ui.js` it finds.

```js
function init(container, idoc) {
  container.innerHTML = '<div class="yourmod-panel">Hello</div>';
}
```

Prefix any `id` you create in `idoc` (WLED's document) with your usermod name. Work inside `container` whenever possible.

## Core dependency

`serveOriginalMainUI()` in `wled_server.cpp` replicates stock `/` handler logic. This is the only non-usermod hook.

## 16.0+ (later)

Replace `file://` with `custom_usermods = usermod_ui` once targeting WLED 16 — see spec §8.4.
