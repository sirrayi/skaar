# Plan P4 (engine side): input and frame feel

BRIEF §9 P4: "Hit sounds, kill flash, uncapped fps, frame pacing, latency overlay, mouse-input verification (cm/360 test)."
Hit sounds and the kill flash belong to the trainer (skaar-07, `plan-P2-P3.md`). This plan covers the engine side.

## Finding: macOS mouse input is accelerated today
The bundled SDL 2.32.8 takes relative mouse motion from NSEvent `deltaX/deltaY`. I checked the dylib: there's no GCMouse use for the mouse.
SDL's own documentation says the NSEvent path includes the system mouse acceleration, and that GCMouse
(GameController framework) is the source of raw, unaccelerated deltas. SDL3 enables GCMouse only on macOS 14+, because motion
and buttons were unreliable on MacBooks under macOS 12/13.
Sources: https://discourse.libsdl.org/t/sdl-base-gcmouse-raw-input-implementation/66840,
https://discourse.libsdl.org/t/sdl-mouse-restrict-gcmouse-to-macos-sonoma-and-later/67940,
https://developer.apple.com/documentation/gamecontroller/gcmouseinput
For an aim trainer that's a correctness bug: cm/360 would depend on how fast you move the mouse.

## Tasks
A. **Raw mouse on macOS 14+**: `code/sys/sys_osx_rawmouse.m`. GCMouse `mouseMovedHandler` on a high-priority serial queue
   accumulates deltas (Y negated to SDL's convention). `sdl_input.c` drops SDL motion while raw input is active and feeds the
   accumulated counts once per frame. SDL still delivers buttons and the wheel. Cvar `in_rawMouse` (default 1).
   On startup the log states which path is active. On the accelerated fallback there's a WARNING toast.
B. **Exact frame limiter**: `com_maxfps` is paced in microseconds (sleep, then spin for the last ~0.3 ms) instead of whole
   milliseconds. 240 is really 240, not 250. Uncapped (0) stays as it is.
C. **Latency and pacing readout** in `dev_overlay`: input age at frame submit (newest mouse event → end of frame), and
   frame time p50/p99 over the last second.
D. **`docs/feel.md`**: the input path per OS, how to verify cm/360 (`sens_test` + `sens_dpi` + a ruler), known limits.

## Done when
- On macOS 14+, the log shows "raw mouse: GCMouse" and `sens_test` counts don't depend on swipe speed (human check with a ruler).
- `com_maxfps 240` shows 240 ± 1 fps in `dev_overlay` with p99 frame time under 4.6 ms.
- `./scripts/smoke.sh` passes.

## File claims (parallel sessions, see CLAUDE.md)
- **skaar-47**: `code/sys/sys_osx_rawmouse.m` (new), the mouse-motion/activation parts of `code/sdl/sdl_input.c`,
  the pacing part of `Com_Frame` in `code/qcommon/common.c`, `code/client/cl_dev.c`, `cmake/platforms/macos.cmake`,
  `docs/plan-P4.md`, `docs/feel.md`.
- Not mine: hit sounds and kill flash (skaar-07), movement (skaar-31, P5).
