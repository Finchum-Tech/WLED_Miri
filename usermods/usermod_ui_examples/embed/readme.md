# Example E: Embed

**Folder:** `embed/` · Part of [Usermod UI examples](../readme.md)

Adds an **Embed** tab containing an existing WLED page in an iframe. The example uses the palette editor at `/cpal.htm`.

**Use this when** the UI you want already exists as a route and you want to show it on the main screen.

**Builds on:** [Example C: New tab](../new_tab/readme.md), using an iframe instead of custom panel content.

## Enable

Requires the [Usermod UI](../../usermod_ui/readme.md) host:

```ini
  -D USERMOD_UI
  -D USERMOD_UI_INCLUDE='"usermod_ui_examples/embed/ui.js"'
```

In WLED v0.15.x, also keep `file://usermods/usermod_ui` in `lib_deps`. See the [Usermod UI README](../../usermod_ui/readme.md) for complete, version-specific setup.

Or enable the [full suite](../readme.md) with `-D USERMOD_UI_EXAMPLES`.

Open [`ui.js`](ui.js), copy it into your usermod, and point the iframe at your own route.

## Notes

- Prevents palette saves from nesting or replacing the main UI.
- When embedding another page, check whether it redirects after an action.
