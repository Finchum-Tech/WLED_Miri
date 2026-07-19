# Example F: Matrix

**Folder:** `matrix/` · Part of [Usermod UI examples](../readme.md)

Adds a **Matrix** tab that draws live LED colors from WLED's liveview stream. A 2D device appears as a grid, while a 1D strip wraps to the panel width.

**Use this when** you want to display live device data inside a tab.

**Builds on:** [Example C: New tab](../new_tab/readme.md), adding live WebSocket data to the panel. This is the most advanced example in the suite.

## Enable

Requires the [Usermod UI](../../usermod_ui/readme.md) host:

```ini
  -D USERMOD_UI
  -D USERMOD_UI_INCLUDE='"usermod_ui_examples/matrix/ui.js"'
```

In WLED v0.15.x, also keep `file://usermods/usermod_ui` in `lib_deps`. See the [Usermod UI README](../../usermod_ui/readme.md) for complete, version-specific setup.

Or enable the [full suite](../readme.md) with `-D USERMOD_UI_EXAMPLES`.

Open [`ui.js`](ui.js) as a reference and copy only the patterns your usermod needs.

## Notes

- Uses WLED's existing main WebSocket rather than opening another connection.
- WLED serves one liveview client at a time. Close Peek if frames do not arrive.
- Liveview starts when the tab is open and stays active while PC Mode shows several panels.
