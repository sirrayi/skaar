# mapc: JSON → Q3 BSP

`tools/mapc` compiles a JSON list of axis-aligned boxes and ramps into a Quake 3 BSP (IBSP v46) that the engine loads like any map.
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
  "ramps": [                                 // wedges: the slope climbs from min.z to max.z toward "rise"
    { "min": [0, 0, 0], "max": [500, 256, 180], "rise": "+x", "material": "block" }  // +x, -x, +y or -y
  ],
  "entities": [                              // arrays become "x y z", everything else a string
    { "classname": "info_player_deathmatch", "origin": [0, 0, 40], "angle": 0 },
    // an entity with its own "brushes"/"ramps" becomes submodel "*N" ("model" is set for you);
    // trigger_* entities get CONTENTS_TRIGGER brushes and draw nothing
    { "classname": "trigger_push", "target": "pad1", "brushes": [{ "min": [0, 0, 4], "max": [128, 128, 36] }] },
    { "classname": "target_position", "targetname": "pad1", "origin": [800, 0, 360] }
  ]
}
```
Units are Quake units (about an inch). The player box is 30×30×56 (mins `-15 -15 -24`, maxs `15 15 32`) and the eye
sits 26 units above the origin, so spawn origins need to be at least 24 above the floor.
Textures repeat every 128 units.

## What it produces
- Brushes with the six axial sides in q3 order (`-x +x -y +y -z +z`), `CONTENTS_SOLID`. A ramp adds its slope as a 7th side.
  Walkable slopes need a normal z of at least 0.7, so up to about 45°.
- One planar surface (quad) per box side. Ramps get a polygon per side (a big quad clipped by the other sides);
  sides squeezed to nothing (a ramp's top and low end) are dropped. All surfaces are vertex-lit
  (`lightmapNum = LIGHTMAP_BY_VERTEX`): ambient + Lambert sun, stored at half intensity (the renderer doubles map light
  via overbright bits).
- A single BSP leaf holding every world brush and surface (collision tests every brush; fine for tens of brushes).
- Model 0 is the world; brush entities follow as models 1..N, each with its own brushes and surfaces (none for triggers),
  outside the world leaf. Trigger brushes use `textures/common/trigger` (`CONTENTS_TRIGGER`, `SURF_NODRAW`).
- One vis cluster that sees itself: renderergl2 skips leafs whose cluster ≥ numClusters, so a map without vis draws nothing.
- A uniform light grid (64×64×128) with the same ambient + sun: renderergl2 derives vertex light directions from it, and models are lit from it.

## Limits / next
- Boxes and axis-aligned ramps only. No rotation, no patches, no lightmaps or shadows, no fog.
- Hidden faces (inside walls, outside the room) are still emitted.
- Brushes must not be needed for vis culling (everything is always drawn).
- No `.aas` for bots (needed by P6).
