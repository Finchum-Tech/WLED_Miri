# Tab layout support

**Folder:** `_tab_layout/` · Part of [Usermod UI examples](../readme.md)

This helper keeps WLED's bottom tab bar usable when several examples add tabs. It does not add a visible tab or panel.

It provides two things:

- Shared panel styles used by the bundled examples.
- A call to the host's `syncTabLayout`, which recounts the tabs and updates the carousel or PC multi-column layout.

It is included automatically with `-D USERMOD_UI_EXAMPLES`. Individual examples do not require it when loaded alone.

Open [`ui.js`](ui.js) when combining several tab-based interfaces and you want to reuse the shared panel helpers.