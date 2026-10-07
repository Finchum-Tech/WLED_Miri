# MiriColorSphere Mod Specification

**Branch:** `mod/color-sphere`  
**Build flag:** `-D USERMOD_MIRI_COLOR_SPHERE`  
**Standalone:** Yes  
**Full spec:** [source/MiriColorSphere_Design_Spec.docx](source/MiriColorSphere_Design_Spec.docx)

## Purpose

3D colour sphere UI replacing WLED's native 2D colour picker. Serves `/sphere` route and integrates into Miri's modified `index.htm`. Pure web UI — no C++ colour logic, no hardware.

## Reference assets

HTML prototypes in [`color-sphere-assets/`](color-sphere-assets/) — use `sphere picker code index.html` or approved variant. Embed final HTML as PROGMEM in `sphere_html.h` unless size exceeds ~40KB.

## Key deliverables

- GET `/sphere` — serves `MIRI_SPHERE_HTML[] PROGMEM`
- Sphere JS POSTs to `/json/state`: `{ "seg": [{ "id": 0, "col": [[R,G,B]] }] }`
- Modified web assets: replace native 2D picker container (not runtime DOM injection)
- `addToJsonInfo()`: `{ "Color Sphere": ["/sphere", "url"] }`
- Optional config: custom route path (default `/sphere`)

## Acceptance criteria

- [ ] Compiles standalone with `USERMOD_MIRI_COLOR_SPHERE`
- [ ] `/sphere` loads 3D picker; colour changes propagate via `/json/state`
- [ ] No MiriState dependency; no Miri branding required in this mod
- [ ] HTML embedded in PROGMEM (or documented LittleFS fallback if >40KB)
- [ ] Serial gated behind `MIRI_DEBUG`


## Implementation status

- [x] Added standalone usermod at `usermods/miri_color_sphere` gated by `USERMOD_MIRI_COLOR_SPHERE`
- [x] Added `GET /sphere` route serving `MIRI_SPHERE_HTML[]` from PROGMEM
- [x] Sphere page posts to `/json/state` as `{"seg":[{"id":0,"col":[[R,G,B]]}]}`
- [x] Added info row exactly as `{ "Color Sphere": ["/sphere", "url"] }`
- [x] Updated source web assets (build-time) with static sphere container integration (no runtime DOM injection)
- [x] Added standalone build env `esp32dev_miri_color_sphere`

### PROGMEM size note

Current `sphere_html.h` payload is ~36.4 KB, below the ~40 KB fallback threshold. LittleFS fallback is not required for this implementation.

### Current validation blocker

`pio` / `platformio` CLI is unavailable in the current shell environment, so local build verification cannot run here.
