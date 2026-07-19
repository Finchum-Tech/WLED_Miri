# Example B: Toast

**Folder:** `toast/` · Part of [Usermod UI examples](../readme.md)

Shows a stock WLED toast as soon as the main UI loads.

**Use this when** you want a one-shot alert or confirmation without adding a permanent control.

**Related:** [Example G: Button Toast](../button_toast/readme.md) shows the same toast from a top-bar button.

## Enable

Requires the [Usermod UI](../../usermod_ui/readme.md) host:

```ini
  -D USERMOD_UI
  -D USERMOD_UI_INCLUDE='"usermod_ui_examples/toast/ui.js"'
```

In WLED v0.15.x, also keep `file://usermods/usermod_ui` in `lib_deps`. See the [Usermod UI README](../../usermod_ui/readme.md) for complete, version-specific setup.

Or enable the [full suite](../readme.md) with `-D USERMOD_UI_EXAMPLES`.

Open [`ui.js`](ui.js), copy it into your usermod, and change the message or error style.

## Notes

- Uses `showToast(text, true)` for the red error style.
- Error toasts do not clear themselves, so this example clears the toast after about 2.9 seconds.
