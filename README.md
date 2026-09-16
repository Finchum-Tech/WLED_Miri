# WLED overlay (Usermod UI)

This repository is **not** a WLED fork. It holds only the custom usermods and overlay files. Build against a pinned [Aircoookie/WLED](https://github.com/Aircoookie/WLED) tag listed in `WLED_VERSION`.

## What is here

| Path | Purpose |
|------|---------|
| `usermods/usermod_ui/` | Iframe host that injects usermod UI into stock WLED without editing `index.htm` |
| `usermods/usermod_ui_examples/` | Example panels (`ui.js`) bundled when `USERMOD_UI_EXAMPLES` is set |
| `platformio_override.sample.ini` | `esp32dev_usermod_ui` env (`USERMOD_UI` + examples) |
| `patches/wled-0.15.1/` | Small WLED **v0.15.x** hooks (register usermod, export server helpers) |

On WLED **v0.16+**, `custom_usermods = usermod_ui` can replace the `usermods_list.cpp` part of the patch. The `wled_server.cpp` exports are still required until those helpers are public upstream.

## Local overlay

```bash
git clone --depth 1 --branch "$(cat WLED_VERSION)" https://github.com/Aircoookie/WLED.git wled
cp -a usermods/usermod_ui usermods/usermod_ui_examples wled/usermods/
cp platformio_override.sample.ini wled/platformio_override.ini
git -C wled apply "$(pwd)/patches/wled-0.15.1/0001-usermod-ui-0.15-hooks.patch"
cd wled && pio run -e esp32dev_usermod_ui
```

See `usermods/usermod_ui/readme.md` for routes and how to add `ui.js` to other usermods.
