# Example D: Repeated panels

**Folder:** `repeated_panels/` · Part of [Usermod UI examples](../readme.md)

Builds several matching tabs from one list. This example creates `bus0`, `bus1`, and `bus2`, each with its own panel and bottom-bar button.

**Use this when** one usermod manages several similar things, such as buses, zones, or sensors.

**Builds on:** [Example C: New tab](../new_tab/readme.md), repeating the same panel and button pattern for each list item.

## Enable

Requires the [Usermod UI](../../usermod_ui/readme.md) host:

```ini
  -D USERMOD_UI
  -D USERMOD_UI_INCLUDE='"usermod_ui_examples/repeated_panels/ui.js"'
```

In WLED v0.15.x, also keep `file://usermods/usermod_ui` in `lib_deps`. See the [Usermod UI README](../../usermod_ui/readme.md) for complete, version-specific setup.

Or enable the [full suite](../readme.md) with `-D USERMOD_UI_EXAMPLES`.

Open [`ui.js`](ui.js), copy it into your usermod, and replace the bus list with your own keys. Each key is included in its panel id.
