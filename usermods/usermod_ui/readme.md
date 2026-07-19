# Usermod UI

Add your own controls, status bars, tabs, and panels to WLED's main page without forking the stock UI or editing WLED core.

Getting started is meant to be easy: enable this usermod, then use one of the ready-made [Usermod UI examples](../usermod_ui_examples/readme.md) as a basis, or write your own UI the same way. Rebuild and flash, and your additions show up on the main page (the default `/` route) alongside everything users are already used to seeing.

Your changes and code stay isolated in your own usermod folder; nothing in the stock WLED `index.htm` needs to change.
Keep reading for the details:

**Setup covered:** WLED v0.15.x and v0.16.0 or later
**Examples:** [usermod_ui_examples](../usermod_ui_examples/readme.md)

---



## Enable

### WLED v0.15.x

Add both entries to your environment. `-D USERMOD_UI` registers the C++ host. The local `lib_deps` entry loads this folder so the Python script can build the JavaScript bundle at compile time.

```ini
build_flags = ${common.build_flags} ${esp32.build_flags}
  -D USERMOD_UI
lib_deps = ${esp32.lib_deps}
  file://usermods/usermod_ui
```

### WLED v0.16.0 and later

Use WLED's custom usermod loading:

```ini
custom_usermods = usermod_ui
```

The local `file://usermods/usermod_ui` entry is only needed for WLED v0.15.x.



**Optional examples:**

Add `-D USERMOD_UI_EXAMPLES` to also ship every [Usermod UI example](../usermod_ui_examples/readme.md). That folder contains only JavaScript (`ui.js` files collected by the bundler). It is not a C++ usermod: no class, no `UsermodManager::add`, and no `USERMOD_ID_*`.

```ini
  -D USERMOD_UI_EXAMPLES
```

For a WLED v0.15.x example environment, copy [platformio_override.sample.ini](platformio_override.sample.ini) to the project root as `platformio_override.ini`, or set:

```ini
[platformio]
default_envs = esp32dev_usermod_ui
```

After editing any `ui.js`, rebuild. For a quick regen without a full compile:

```bash
python usermods/usermod_ui/tools/regen_bundle_dev.py
```

### What you get on the device


| Route                       | Purpose                                               |
| --------------------------- | ----------------------------------------------------- |
| `GET /`                     | Wrapper page (iframe + bundle + injector)             |
| `GET /wled_orig`            | Stock main UI                                         |
| `GET /usermod_inject.js`    | Title/favicon mirror, navigation fix, module dispatch |
| `GET /usermod_ui_bundle.js` | Pre-gzipped compile-time UI bundle                    |


`/wled_orig` is served by this usermod using the same stock path as core's `GET /` (captive portal, welcome page, or `PAGE_index`). A custom `index.htm` uploaded via `/edit` still wins through `handleStaticContent` (stock filesystem override behavior).

---



## Add UI to your usermod (authors)

1. Enable **Usermod UI** using the version-specific setup above, then enable your own usermod normally (`-D USERMOD_YOUR_NAME` on WLED v0.15.x).
2. Add `ui.js` under your usermod folder. The bundler discovers every `ui.js` inside **enabled** usermods.

```
usermods/your_usermod_name/
  ├── your_usermod_name.cpp   (or .h)
  ├── library.json           (if you have one)
  └── ui.js                  ← add this
```

You can also split across subfolders. Each `ui.js` becomes its own isolated module:

```
usermods/your_usermod_name/
  ├── panel/ui.js
  ├── controls/ui.js
  └── …
```

Skipped automatically: `node_modules/`, `vendor/`, `third_party/`, and dot-directories.

**Escape hatch:** bundle only explicit paths and skip automatic scanning:

```ini
  -D USERMOD_UI_INCLUDE='"your_usermod_name/panel/ui.js,your_usermod_name/controls/ui.js"'
```

Then define an `init` entry point. No imports or registration API are needed:

```js
function init(container, idoc) {
  // Prefer working inside `container`; it is private to this module.
  container.innerHTML = '<div class="yourmod-panel">Hello</div>';
}
```



### `container` vs `idoc`


| Argument    | What it is                                     | When to use it                                             |
| ----------- | ---------------------------------------------- | ---------------------------------------------------------- |
| `container` | A private `<div>` for your module              | Panels, readouts, and controls you own; collision-safe     |
| `idoc`      | The live stock WLED document inside the iframe | Anchoring into WLED structure (tabs, `#top`, `#bot`, etc.) |


`init` runs each time the main UI (or a settings sub-page) finishes loading. The host reuses your `container`. If you insert nodes into `idoc`, guard against double-insertion (e.g. check `getElementById` before creating).

Your file is wrapped in an isolated scope, so local function names never collide with other usermods.

### ID prefix convention

Inside `container`, any class names are fine. When you create elements in `idoc`, prefix every `id` with your usermod name (`yourmod-tab`, `yourmod-picker`). `getElementById` returns the first match silently, so unprefixed IDs can collide across usermods.

Sharing WLED classes (`tabcontent`, `tablinks`, …) is encouraged for a native look.

### Tiers of UI work

- **Basic:** stay in `container` (status, simple panel). Safest and enough for most mods.
- **Medium:** make one or two changes in `idoc` (new tab, button, or embedded page).
- **Advanced:** hide or replace stock controls. This couples tightly to core markup, so prefer additive UI.

See the [Usermod UI examples](../usermod_ui_examples/readme.md). Start with `demo_bar`, then choose the pattern you need: `toast`, `new_tab`, `repeated_panels`, `embed`, `matrix`, or `button_toast`.

---



## What you do not need to worry about

- Editing or forking WLED's `index.htm` / `PAGE_index`
- Other usermods' JS (each module is scoped)
- Shipping a second copy of the stock UI; contributions share one gzipped bundle
- Manual bundler wiring beyond enabling this usermod



## Current limits

- Additive UI is the supported path; replacing stock controls is possible but fragile
- No placement coordinator between usermods (each appends its own UI)
- Automatic discovery has no per-file disable flag; use `USERMOD_UI_INCLUDE` to bundle only explicit `ui.js` paths
- Bundler only scans local `usermods/` folders (not git-URL / external libdeps usermods yet)

