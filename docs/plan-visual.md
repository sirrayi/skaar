# Visual pass: light range, chalk targets, quieter HUD

Asked for by the human 2026-10-22: a greyish taupe off-white ground, flat pastel-red targets (no chrome look),
a minimal HUD, and Manrope as the UI face.

## Palette (`game/shared/theme.h`)
| Token | Value | Use |
|---|---|---|
| `kStage` | `#e6e3dd` | ground: menus, overlays |
| `kCream` | `#232323` | ink (Soft 62%, Faint 34%) |
| `kEmber` | `#ea9692` | pastel chalk red: targets, selected item, PBs |

The token names are from the earlier dark theme and are kept so callers don't change.

## Range
- `range_*` textures are light taupe (`tools/gen_assets.py`). `data/maps-src/range.json` bakes ambient 0.47 + sun 0.05:
  the engine doubles vertex light (overbright), so this lands near the texture values.
- The GL2 renderer's HDR auto-exposure pulled the light room to mid-grey and turned the targets maroon.
  The trainer sets `r_autoExposure 0` on every cgame load (archived configs hold the old `1`) and keeps `r_toneMap 1`:
  with `r_hdr 1` (the default) and the tonemap off, everything blows out to white. Checked with `r_hdr` 1 and 0.
- Targets: `rgbGen entity` times a nearly flat matcap (`target_shade.tga`, 6% darkening at the very rim), so they
  read as matte chalk. A hit blanches the target briefly.

## Type
- Manrope (OFL, `tools/fonts/manrope/`) replaces Montserrat: `manrope-200` (display) and `manrope-300` (text).
  Digits are tabular in both atlases so timers and scores don't jitter. JetBrains Mono stays for menu metadata.

## HUD
- Run: the score at the top centre, then time and accuracy on one quiet line below. No labels, bar or footer.
- Countdown: scenario name in sentence case, description and duration on one line, the count, best score.
- Free roam: speed and the map/clock line in Manrope, faint. Crosshair defaults to ink (`232323`);
  the old cream default `ebe6db` is treated as "default".

## File claims (done, released)
skaar-c6: `game/shared/theme.h`, `draw2d.cpp` (font names), `game/cgame/hud.cpp`, the drawing parts of
`game/cgame/trainer.cpp` (cleared with skaar-07), `tools/gen_assets.py`, `tools/fonts/manrope/`,
`data/base/fonts/manrope-*`, `skaar-ui.shader`, `range_*`, `target_shade.tga`, `cursor.tga`, `range.json` light.
