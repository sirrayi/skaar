# mapc: JSON → Q3 BSP

`tools/mapc` compiles a JSON list of axis-aligned boxes into a Quake 3 BSP (IBSP v46) that the engine loads like any map.
It exists so we have test maps and simple aim-trainer arenas without a q3map2 toolchain.

The build compiles every `data/maps-src/<name>.json` into `<build>/base/generated.pk3dir/maps/<name>.bsp`.
dev-watch rebuilds and reloads the map on save.

## Format
```jsonc
// comments are allowed
{
  "name": "testroom",
  "message": "Skaar Test Room",             // worldspawn message
  "materials": {                             // material key -> texture path (no extension)
    "floor": "textures/skaar/grid_floor"     // unlisted keys map to textures/skaar/<key>
  },
  "light": { "ambient": 0.5, "sun": 0.5, "sunDir": [0.4, 0.25, -1] },
  "rooms": [                                 // hollow boxes: six wall brushes around the interior
    { "min": [-1024, -1024, 0], "max": [1024, 1024, 512], "wall": 32,
      "floor": "floor", "walls": "wall", "ceiling": "ceiling" }
  ],
  "brushes": [                               // solid boxes
    { "min": [384, -64, 0], "max": [512, 64, 128], "material": "block" }
  ],
  "entities": [                              // arrays become "x y z", everything else a string
    { "classname": "info_player_deathmatch", "origin": [0, 0, 40], "angle": 0 }
  ]
}
```
Units are Quake units (about an inch). The player box is 30×30×56 (mins `-15 -15 -24`, maxs `15 15 32`) and the eye
sits 26 units above the origin, so spawn origins need to be at least 24 above the floor.
Textures repeat every 128 units.

## What it produces
- Brushes with the six axial sides in q3 order (`-x +x -y +y -z +z`), `CONTENTS_SOLID`.
- One planar surface (quad) per brush side, vertex-lit (`lightmapNum = LIGHTMAP_BY_VERTEX`): ambient + Lambert sun, stored at half intensity (the renderer doubles map light via overbright bits).
- A single BSP leaf holding every brush and surface (collision tests every brush; fine for tens of brushes).
- One vis cluster that sees itself: renderergl2 skips leafs whose cluster ≥ numClusters, so a map without vis draws nothing.
- A uniform light grid (64×64×128) with the same ambient + sun: renderergl2 derives vertex light directions from it, and models are lit from it.

## Limits / next
- Axis-aligned boxes only. No rotation, no patches, no lightmaps or shadows, no fog.
- Hidden faces (inside walls, outside the room) are still emitted.
- Brushes must not be needed for vis culling (everything is always drawn).
- No `.aas` for bots (needed by P6).
