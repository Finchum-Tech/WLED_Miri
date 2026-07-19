# Example G: Button Toast

**Folder:** `button_toast/` · Part of [Usermod UI examples](../readme.md)

Adds a stock-looking top-bar button. Click it to show a WLED toast.

**Use this when** you want to add a small action to WLED's top button row.

**Related:** [Example B: Toast](../toast/readme.md) shows the same toast on page load.

## Enable

Requires the [Usermod UI](../../usermod_ui/readme.md) host:

```ini
  -D USERMOD_UI
  -D USERMOD_UI_INCLUDE='"usermod_ui_examples/button_toast/ui.js"'
```

In WLED v0.15.x, also keep `file://usermods/usermod_ui` in `lib_deps`. See the [Usermod UI README](../../usermod_ui/readme.md) for complete, version-specific setup.

Or enable the [full suite](../readme.md) with `-D USERMOD_UI_EXAMPLES`.

Open [`ui.js`](ui.js), copy it into your usermod, and point the click handler at your own action.

## Notes

- Uses the same icon and label structure as WLED's stock top buttons.
- Uses `showToast(..., true)` and clears the error style after about 2.9 seconds.
- For a full real-world replacement of stock controls, see Miri ColorSphere in the [WLED_Miri repository](https://github.com/Finchum-Tech/WLED_Miri/).
