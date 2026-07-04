# UI Wrapper usermod

Host usermod that serves the stock WLED control UI inside a same-origin `<iframe>` and exposes a plugin registry for other usermods to inject DOM/JS/CSS without modifying `index.htm`.

See `wled-ui-wrapper-usermod-spec.md` for the full architecture.

## Enable

Add to your `platformio_override.ini` (or build environment):

```ini
build_flags =
  ${common.build_flags}
  -D USERMOD_UI_WRAPPER
```

Optional demo plugin (adds a small status banner to validate the registry):

```ini
  -D USERMOD_UI_WRAPPER_DEMO
```

Or use the bundled PlatformIO environment:

```ini
[platformio]
default_envs = esp32dev_ui_wrapper
```

## Routes

| Route | Purpose |
|---|---|
| `GET /` | Wrapper page (iframe + injector scripts) |
| `GET /wled_orig` | Stock main UI passthrough (`PAGE_index` or LittleFS override) |
| `GET /usermod_inject.js` | Base injector (title/favicon mirror, `getURL()` override, plugin runner) |

## Plugin API (C++)

From another usermod's `setup()`:

```cpp
#include "../usermod_v2_ui_wrapper/usermod_v2_ui_wrapper.h"

void MyUsermod::setup() {
  UIWrapperUsermod::registerInjector("/my_usermod_ui.js");
  server.on("/my_usermod_ui.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, FPSTR(CONTENT_TYPE_JAVASCRIPT), FPSTR(MY_INJECTOR_JS));
  });
}
```

Register the host usermod **before** plugin usermods in `usermods_list.cpp` is not required — wrapper HTML is assembled at request time after all `setup()` calls complete.

## Plugin API (JavaScript)

```js
window.WLEDUI.register(function (idoc) {
  if (idoc.getElementById('my-plugin-panel')) return; // idempotency guard
  // inject into idoc (iframe document)
});
```

Prefix element IDs with your usermod name to avoid collisions between plugins.
