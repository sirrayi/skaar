# Input and frame feel

What the engine guarantees about mouse input and frame pacing, and how to check it yourself. Plan: `docs/plan-P4.md`.

## Mouse input path
| OS | Source | Acceleration |
|---|---|---|
| macOS 14+ | **GCMouse** raw counts (`code/sys/sys_osx_rawmouse.m`) | none |
| macOS 11–13 | SDL 2 relative mode (NSEvent deltas) | **system acceleration applies** |
| Windows | SDL 2 relative mode (Win32 raw input) | none |
| Linux | SDL 2 relative mode (XInput2 raw / Wayland relative pointer) | none |

- On macOS, SDL 2 takes relative motion from NSEvent deltas, which carry the system pointer acceleration. We checked the bundled `libSDL2` binary: it doesn't use GCMouse for the mouse.
  GCMouse reports raw device counts. SDL 3 uses it only from macOS 14, because motion and buttons were unreliable on MacBooks under 12/13.
  Sources: [SDL GCMouse raw input](https://discourse.libsdl.org/t/sdl-base-gcmouse-raw-input-implementation/66840),
  [SDL: restrict GCMouse to Sonoma+](https://discourse.libsdl.org/t/sdl-mouse-restrict-gcmouse-to-macos-sonoma-and-later/67940),
  [Apple GCMouseInput](https://developer.apple.com/documentation/gamecontroller/gcmouseinput).
- Only motion comes from GCMouse. Buttons and the wheel still come through SDL, so there are no duplicates.
- `in_rawMouse 1` (default) enables it; it takes effect after an input restart. The console logs which path is active at startup:
  `mouse: raw input (GCMouse), no OS acceleration`, or a WARNING when the input is accelerated.
- Raw counts are what other games' sensitivity converters assume (Windows raw input). Moving from the old NSEvent path to
  GCMouse therefore changes how sensitivity feels: 1 count is now one physical sensor count.

## Sensitivity math
Yaw per count = `sensitivity × m_yaw` degrees (`m_yaw` 0.022 by default, the classic arena value), with no filtering or accel (`m_filter 0`,
`cl_mouseAccel 0` in default.cfg). So counts/360 = 360 / (sensitivity × m_yaw) and cm/360 = counts/360 / dpi × 2.54.

### Verify it yourself (`sens_test`)
1. `sens_dpi <your mouse dpi>` (once; it's saved).
2. Open the console, type `sens_test`, close the console.
3. Turn exactly one full turn (line the crosshair up with a mark), or move the mouse along a ruler for, say, 10 cm.
4. Open the console and type `sens_test` again. It prints the counts, the degrees turned, measured vs expected counts/360, cm/360, and
   **how far the mouse moved according to its counts**. If that matches your ruler at both slow and fast swipes, there's no acceleration.

## Frame pacing
- `com_maxfps 0`: uncapped (the engine still waits at least 1 ms per frame, as upstream).
- `com_maxfps N`: paced in **microseconds** with fixed deadlines (`Com_PaceFrame` in common.c), so 240 really means 240, not the
  250 that upstream's whole-millisecond loop gives. It sleeps while more than ~1.5 ms remain and spins the rest.
  `com_busyWait 1` spins the whole time (lowest jitter, one core at 100%).
- `r_swapInterval 0` (default.cfg) disables vsync. On macOS the GL driver can still block in the swap once per display refresh.
  That shows as ~8.3 ms frames at 120 Hz and is not a hitch (why `dev_spikeMinMs` defaults to 12).

### See it (`dev_overlay 1`, or `dev_stats` in the console)
- `fps`, frame time p50 / p99 over the last second
- `input->swap`: from the newest mouse motion a frame used to the end of that frame (after the buffer swap). This is the
  engine's share of input latency. Display scan-out and the panel come after that and can't be measured in software.
- `mouse`: the active input path

## Not verified yet (needs a human at the machine)
- `com_maxfps 240` holding 240 ± 1 with p99 < 4.6 ms. My automated test windows kept getting closed before they could measure.
- Swipe-speed independence of the counts with a ruler (above).

## Performance budget (Phase 2 D)
Target on the reference machine (MacBook Pro M2 Pro, 120 Hz panel), measured with `dev_stats` or `dev_overlay 1`,
**foreground window** (macOS throttles background windows, so agent runs can't measure this):

| Situation | `com_maxfps` | Must hold |
|---|---|---|
| Any trainer scenario, HUD on | 240 | fps 240 ± 2, frame p99 ≤ 4.6 ms |
| Arena (`devmap duel`) with dummies and rockets | 240 | fps 240 ± 2, frame p99 ≤ 4.6 ms |
| Menus / Insights (2D UI) | 240 | fps 240 ± 2, frame p99 ≤ 4.6 ms |
| Uncapped (`com_maxfps 0`) | 0 | no spikes in the log over 5 min |

A soak run (`scripts/soak.sh`) also records frame p99 per sample in `logs/soak-*.csv` for regressions over time.
