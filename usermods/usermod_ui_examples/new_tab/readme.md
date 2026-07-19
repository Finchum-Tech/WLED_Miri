# Example C: New tab

**Folder:** `new_tab/` · Part of [Usermod UI examples](../readme.md)

Adds one bottom-bar button and one matching panel: the basic pattern for giving a usermod its own tab.

**Use this when** you need a dedicated place for controls or status alongside Colors, Effects, and Presets.

**Next:** [Example D: Repeated panels](../repeated_panels/readme.md) creates several tabs, [Example E: Embed](../embed/readme.md) uses an iframe, and [Example F: Matrix](../matrix/readme.md) adds live data.

## Enable

Requires the [Usermod UI](../../usermod_ui/readme.md) host:

```ini
  -D USERMOD_UI
  -D USERMOD_UI_INCLUDE='"usermod_ui_examples/new_tab/ui.js"'
```

In WLED v0.15.x, also keep `file://usermods/usermod_ui` in `lib_deps`. See the [Usermod UI README](../../usermod_ui/readme.md) for complete, version-specific setup.

Or enable the [full suite](../readme.md) with `-D USERMOD_UI_EXAMPLES`.

Open [`ui.js`](ui.js), copy it into your usermod, and change the tab label and panel content.

## Notes

- Adds a `tabcontent` panel and a bottom-bar button, then opens the panel with `openTab`.
