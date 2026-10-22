# Project Brief: Open Aim Trainer + Arena Engine (working name: Skaar)

Read this fully before writing any code. Work through MILESTONES in order. Do not skip ahead.

## 1. Vision
A cross-platform (macOS, Linux, Windows) C++ aim trainer in the spirit of KovaaK's, built on a real FPS engine so it can also host:
1. **Aim-trainer scenarios** (tracking, flicking, clicking, target switching) with stats, replays, leaderboards later.
2. **Arena-shooter mode** (Reflex Arena-style): fast movement, hitscan/projectile weapons, bots/dummies, duel maps.
3. **CPMA-style movement ruleset** (Quake 3 / Challenge ProMode Arena physics: air control, strafe jumping, double-jump ramps).
4. **Tac-shooter aim modes** modeled on Valorant / CS:GO feel (recoil patterns, movement accuracy, counter-strafing, weapon spread).
5. **Surf engine**: Source-style ramp sliding with air-strafe physics.

Everything must be ORIGINAL work: own names, own maps, own art. Do not use trademarked names (Valorant, CS:GO, CPMA, Reflex, KovaaK's) in the product. Use them only as "inspired by" in internal notes.

## 2. Hard constraints
- Language: C++20. Build: CMake. Package deps via vcpkg or CPM.cmake (pick one, document it).
- Must build and run on macOS (Apple Silicon, primary dev machine: MacBook Pro M2 Pro), Linux, Windows.
- Fixed-timestep simulation (target 125 Hz or higher tick, decoupled from render). Deterministic sim so replays work.
- Raw mouse input, no OS acceleration. Sensitivity must be cm/360 accurate and convertible from other games.
- Low latency is the product. Uncapped FPS, optional vsync off, minimal input-to-photon delay.
- Licensing: do NOT copy code from id Tech 3 (GPL), Source SDK, or KovaaK's. Reading public docs and papers on movement physics is fine. If we ever want to fork ioquake3, that is a deliberate GPL decision made by the human, not the agent.

## 3. Recommended stack (change only with a written reason)
| Concern | Choice | Why |
|---|---|---|
| Window/input/audio events | SDL3 | Cross-platform, raw mouse, gamepad |
| Rendering | OpenGL 4.1 core first (Metal-compatible via macOS limit), abstracted behind a thin renderer interface | macOS caps OpenGL at 4.1; keep a path to bgfx or Vulkan/MoltenVK later |
| Math | glm | Standard |
| ECS / entities | EnTT | Fast, simple |
| UI / menus / HUD | Dear ImGui | Quick tooling and settings screens |
| Audio | miniaudio | Single header, cross-platform |
| Physics / collision | Own code: swept AABB vs BSP-style brush/triangle world | Movement feel needs custom control; do NOT use a generic rigid-body engine for player movement |
| Maps | Custom simple format first (brush list in JSON), later glTF import | Avoid the BSP compiler rabbit hole early |
| Serialization | nlohmann/json for configs, binary for replays | |
| Tests | Catch2 | Sim determinism tests |

## 4. Architecture
```
/engine   core loop, input, renderer, audio, assets, math
/sim      deterministic game sim: player movement, weapons, hit detection, entities
/game     modes (scenarios, arena, tac, surf), rulesets, HUD
/tools    map converter, replay viewer, sens converter
/data     scenarios, maps, configs (JSON)
/tests    determinism + movement unit tests
```
Key design rule: **movement and weapons are "rulesets"** (plug-in structs), so Quake-style, tac-style, and surf-style physics are swappable data + code, not forks of the engine.

Interfaces to define early:
- `IRuleset` (movement step, weapon fire, damage)
- `IMode` (scenario logic, scoring, win condition)
- `IRenderer` (draw calls abstracted)

## 5. MILESTONES (do one at a time, each ends with something runnable)
**M0 – Skeleton.** CMake project, SDL3 window, render a colored cube room, raw mouse look, WASD, fixed timestep loop with FPS/tick counter. Builds on macOS first, CI for Linux/Windows after.
**M1 – Aim trainer core.** Spheres spawn in a room, hitscan click, hit/miss stats, accuracy %, simple scenario file (JSON): static click, tracking bot, strafing bot. Sens settings with cm/360 and game-sens converter.
**M2 – Feel pass.** Crosshair options, FOV, hit sounds, kill flash, frame pacing, input latency measurement overlay. This is where it either feels good or doesn't.
**M3 – Scenario system + stats.** Scenario browser, per-run stats saved locally, score formula, replay recording (inputs + seed).
**M4 – Quake-style ruleset.** Player controller with accel/friction, strafe jumping, air control, ramp handling, jump pads. Test maps for movement. Write unit tests for known behaviors (speed gain in strafe jumps).
**M5 – Arena mode.** Weapons (rocket, rail, lightning, shotgun), pickups, bot dummies, a duel map.
**M6 – Tac ruleset.** Recoil patterns, spread, movement penalty, counter-strafe, bots on aim ranges.
**M7 – Surf.** Source-style collision slide on angled planes, air accelerate with low air cap, surf ramp test maps.
**M8 – Polish + multiplayer research.** Netcode is OUT OF SCOPE until M7 is done. Then decide: client-side prediction + server reconciliation.

## 6. Reference material the agent should research (and summarize into /docs before using)
- Quake 3 / CPMA movement: "Quake 3 movement physics explained", Quake Wiki strafe jumping, CPMA air control docs (read for behavior, not for copying code).
- Source/CS surf movement: Source engine `AirAccelerate`, sv_airaccelerate, sv_accelerate, clipping on ramps (public writeups, e.g. Adrian Courrèges-style breakdowns, GDC talk "Source movement").
- Aim trainer design: KovaaK's and Aim Lab scenario types and scoring concepts; sensitivity conversion math (cm/360, yaw values per game).
- Fixed timestep: Glenn Fiedler "Fix Your Timestep".
- Swept AABB collision: Quake-style trace and clip-velocity algorithm (conceptually).
- Hit registration and lag compensation (for later).

## 7. Working rules for the agent
- Small commits, one feature each. Always leave the build green.
- Before each milestone: write a short plan in /docs/plan-MX.md and wait for human sign-off if scope is unclear.
- Never add a dependency without listing it and the reason in /docs/deps.md.
- Every movement or weapon change needs a test or a recorded replay that demonstrates it.
- Ask the human when: choosing licensing-sensitive options, changing the stack, or the "feel" is subjective and needs a play-test.
- Report format after each task: what runs now, how to run it, what's next. Keep it short.

## 8. Open questions for the human (answer before M4)
- Solo hobby, open source, or commercial product? (Affects licensing and dependencies.)
- Multiplayer wanted, or single-player aim practice only at launch?
- Target launch platform to optimize first (assume macOS dev, Windows for the audience).

---

## 9. UPDATE (2026-10-22): CURRENT SCOPE. This section OVERRIDES sections 2 (licensing), 3 (stack) and 5 (milestones) where they conflict.

### Decisions made by the human
- Fork **ioquake3** (https://github.com/ioquake/ioq3, GPLv2) as the base. Reuse and improve its engine and game code heavily. This is the deliberate GPL choice: the whole project is **GPLv2-or-later**, source stays public. Add a LICENSE file and keep all original copyright headers.
- New code (aim trainer game module, lobby, tools) is written in **C++** where practical (cgame/game modules built as native libs, not QVM). Engine core stays C.
- Build and test on **macOS (Apple Silicon M2 Pro) first**. Linux (Ubuntu) is verified later by the human. Keep Windows buildable.
- Scope right now: **(1) solo lobby, (2) aim trainer that feels great, (3) first working CPM-style movement mode.** Arena weapons, tac rulesets, surf and netcode come after.
- The old from-scratch SDL3 engine plan is shelved, not deleted. Revisit after this phase.

### Legal guardrails (do not violate)
- **CPMA itself is NOT open source.** Its mod code and assets belong to its authors. Do not copy it, decompile it, or ship it. Implement "CPM-style" movement from documented behavior (Quake wiki / community physics write-ups) by modifying the GPL Q3 `bg_pmove.c` logic, then verify against known behavior with tests and replays.
- **Q3 game assets (pak0.pk3) are proprietary.** Never commit or ship them. The project must run on free/own assets: placeholder maps, textures, sounds, player models. The ioq3 repo ships without them.
- Do not use the names Quake, CPMA, Valorant, CS:GO, KovaaK's or Reflex in the product name or UI.

### Phase 1 milestones (one at a time, each ends runnable on macOS)
**P0. Build baseline.** Clone ioq3, build native on macOS arm64 with the OpenGL2 renderer, SDL2. Document exact commands in /docs/build-macos.md. Get it launching to the console with placeholder assets. Rename the product, keep GPL headers.
**P1. Own game module skeleton.** Replace baseq3 game/cgame/ui with our own module (C++ ok), loaded as a native lib. Boots into an empty test room map.
**P2. Solo lobby.** A main menu and in-game lobby screen: pick a scenario/mode, sensitivity settings (cm/360 + convert from other games), crosshair, FOV, start/restart. Scenario list is data-driven (JSON or cfg).
**P3. Aim trainer v1.** Target entities (spheres), hitscan, hit/miss stats, accuracy %, timer, score, end-of-run summary saved to disk. Scenarios: click static, tracking bot, strafing bot, target switching. Raw input on, no mouse accel.
**P4. Feel pass.** Hit sounds, kill flash, uncapped fps, frame pacing, latency overlay, mouse-input verification (cm/360 test).
**P5. CPM-style movement v1.** New physics ruleset in the game module's pmove: strafe jumping, higher air control, double jump / ramp jump behavior, CPM-style accelerate/friction values, selectable per mode (vq3 vs cpm). Include a movement test map (ramps, jump pad, long straight) and a replay/unit test that checks speed gain from strafe jumping. Research the exact constants from public docs first and write them in /docs/cpm-physics.md with sources before coding.
**P6. CPM duel/practice mode.** One test arena map, dummy bots, basic weapons (rail, rocket, lightning, shotgun) using Q3's existing weapon code.

### Research the agent must do first (summarize into /docs before coding)
1. ioq3 repo layout (code/game, code/cgame, code/ui, code/client, code/renderergl2), and its macOS build instructions (Makefile and CMake paths, SDL2 dependency).
2. How to build game modules natively (not QVM) in ioq3 and load them (`vm_game 0`, `vm_cgame 0`, `vm_ui 0`).
3. Q3 `bg_pmove.c` movement flow (PM_Accelerate, PM_AirMove, PM_WalkMove, PM_CheckJump, PM_SlideMove) and how CPM differs (air control, strafe accel, double jump, ramp jumps).
4. Free asset sources with compatible licenses (OpenArena assets are GPL; check each license) for placeholder maps/models/sounds.

### Rules for this phase
- One milestone at a time. Write /docs/plan-PX.md first, then build, then report: what runs, how to run it, what's next.
- Every change must build on macOS. If a macOS-only problem blocks progress, stop and tell the human the exact error.
- Commit small. Keep the build green.
- Read DEV_ENV.md right after P0. Do task P0.5 (all of DEV_ENV.md Part B, in order) BEFORE starting P1. The human tests with `./scripts/dev-watch.sh` (auto rebuild, relaunch, live errors). No gameplay work until that loop works.
