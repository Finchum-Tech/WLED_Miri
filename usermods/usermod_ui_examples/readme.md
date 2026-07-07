# usermod_ui_examples

Reference recipes for the Usermod UI framework (spec §8.7–8.8). One `ui.js` per recipe - enable everything with a single flag:

```ini
-D USERMOD_UI_EXAMPLES
```

| Subfolder | Recipe | Module key |
|-----------|--------|------------|
| `demo_bar/` | A - header status bar (read this first) | `usermod_ui_examples__demo_bar` |
| `toast/` | B - floating toast | `usermod_ui_examples__toast` |
| `new_tab/` | C - new bottom-bar tab | `usermod_ui_examples__new_tab` |
| `repeated_panels/` | D - multiple tab panels | `usermod_ui_examples__repeated_panels` |
| `embed/` | E - iframe existing page | `usermod_ui_examples__embed` |
| `matrix/` | F - live 5×5 WebSocket grid | `usermod_ui_examples__matrix` |

Bundler-only - no C++ class, no `UsermodManager::add`, no `USERMOD_ID_*`.

Bundle a subset only:

```ini
-D USERMOD_UI_INCLUDE='"usermod_ui_examples/demo_bar/ui.js,usermod_ui_examples/matrix/ui.js"'
```
