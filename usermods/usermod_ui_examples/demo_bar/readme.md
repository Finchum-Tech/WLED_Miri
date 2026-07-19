# Example A: Demo bar

**Folder:** `demo_bar/` · Part of [Usermod UI examples](../readme.md)

Adds a small status strip under WLED's top button row. It is the simplest example and a good place to start.

**Use this when** you want an always-visible status or banner in the header.

**Next:** [Example B: Toast](../toast/readme.md) uses a stock WLED helper without adding permanent controls.

## Enable

Requires the [Usermod UI](../../usermod_ui/readme.md) host:

```ini
  -D USERMOD_UI
  -D USERMOD_UI_INCLUDE='"usermod_ui_examples/demo_bar/ui.js"'
```

In WLED v0.15.x, also keep `file://usermods/usermod_ui` in `lib_deps`. See the [Usermod UI README](../../usermod_ui/readme.md) for complete, version-specific setup.

Or enable the [full suite](../readme.md) with `-D USERMOD_UI_EXAMPLES`.

Open [`ui.js`](ui.js), copy it into your usermod, and adapt the label text and styling.

## Notes

- Calls `size()` so the page content clears the new strip.
