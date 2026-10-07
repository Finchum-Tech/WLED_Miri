# Miri Color Sphere Usermod

Standalone usermod for serving a sphere-based color picker at `/sphere`.

## Enable

- Add `miri_color_sphere` to `custom_usermods`
- Build with `-D USERMOD_MIRI_COLOR_SPHERE`

Example PlatformIO environment:

```ini
[env:esp32dev_miri_color_sphere]
extends = env:esp32dev
custom_usermods = miri_color_sphere
build_flags = ${env:esp32dev.build_flags}
  -D USERMOD_MIRI_COLOR_SPHERE
```

## Runtime behavior

- Registers `GET /sphere` and serves `MIRI_SPHERE_HTML` from PROGMEM.
- Sphere page posts color updates to `/json/state` using:
  - `{"seg":[{"id":0,"col":[[R,G,B]]}]}`
- Publishes info row through `addToJsonInfo()`:
  - `{ "Color Sphere": ["/sphere", "url"] }`

## UI integration

Core `index.htm` and `index.js` include a static `#spherewrap` container and toggle it only when `Color Sphere` is present in `/json/info`. Non-mod builds keep native WLED color picker behavior.
