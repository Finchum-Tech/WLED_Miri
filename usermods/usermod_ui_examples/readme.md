# Usermod UI examples

A bundled showcase of what [Usermod UI](../usermod_ui/readme.md) can add to WLED's main control page. Enable the examples folder to load every example at once. Each example is also a standalone `ui.js` that can be loaded or copied without the others.

**Enable:** with the [Usermod UI host configured for your WLED version](../usermod_ui/readme.md), add `-D USERMOD_UI_EXAMPLES` for every example at once, or `-D USERMOD_UI_INCLUDE='"usermod_ui_examples/demo_bar/ui.js"'` (comma-separate paths under `usermods/` for a subset). Rebuild after any `ui.js` change.

## What each example shows

Letters follow a suggested learning order, from a simple status bar to tabs and live data.

| Example | Folder | What you see |
|---------|--------|----------------|
| **A** · Demo bar | [`demo_bar/`](demo_bar/readme.md) | A status strip under the top buttons saying Usermod UI is active |
| **B** · Toast | [`toast/`](toast/readme.md) | A stock toast notification on page load |
| **C** · New tab | [`new_tab/`](new_tab/readme.md) | One new bottom-bar tab with its own panel |
| **D** · Repeated panels | [`repeated_panels/`](repeated_panels/readme.md) | Several similar bottom-bar tabs from one list |
| **E** · Embed | [`embed/`](embed/readme.md) | A tab that iframes an existing WLED page (palette editor) |
| **F** · Matrix | [`matrix/`](matrix/readme.md) | A tab with a live LED grid/strip over the main WebSocket |
| **G** · Button Toast | [`button_toast/`](button_toast/readme.md) | A stock-looking top-bar button that shows a toast when clicked |

Start with **Example A** ([`demo_bar`](demo_bar/readme.md)). Open any folder's `readme.md` for that example's include line and details.

## Supporting helper

[`_tab_layout/`](_tab_layout/readme.md) is not an example. It provides shared layout helpers for keeping the bottom tab bar usable when several examples add tabs.