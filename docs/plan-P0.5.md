# Plan P0.5: dev environment and live error visibility

Implements `docs/DEV_ENV.md` Part B (B1–B9), in order. No gameplay work until `./scripts/dev-watch.sh` works.

## Mapping DEV_ENV paths onto the ioq3 fork
DEV_ENV was written for the from-scratch layout (`engine/`, `sim/`, `game/`). In the fork:
- engine C code, including the current stock game modules: `code/`
- our C++ game modules (from P1): `game/`
- tools: `tools/`, data: `data/`, CMake: `cmake/`
The watcher monitors `code/ game/ tools/ cmake/ CMakeLists.txt data/`.

## Design decisions
- **Build flags (B1).** `-Wall -Wextra -Werror` apply only to our sources (new files in `code/`, plus `game/`, `tools/`, `tests/`).
  Upstream ioq3 keeps its own flags; otherwise the watcher would drown in upstream warnings. Dev builds turn LTO off for faster links.
- **`SKAAR_DEV` define.** Set by `scripts/build.sh` for both `fast` and `safe`. `DEV_ASSERT` and the dev overlay are compiled out without it (release builds).
- **Toasts and overlay (B4)** live in the engine client (`code/client/cl_dev.c`), not cgame, so they work even when a game module fails to load.
  Classification hooks `Com_Printf`: lines containing `WARNING`, `ERROR`, `ASSERT` or `spike`.
- **Fatal errors (B4).** `ERR_DROP` shows a full-screen red panel for 10 s (or until the next map load) with the log path.
  `ERR_FATAL` already shows a native dialog (`Sys_ErrorDialog`); we add the log and crash-file paths to it. `dev_errorDialog 0` suppresses it for headless runs.
- **Crash handler (B5).** Extends ioq3's `Sys_SigHandler` to write `logs/crash-<timestamp>.txt` first, then do the normal shutdown.
- **Data reload (B9).** dev-watch sends `dev_reload <file>` to the running game through ioq3's existing `com_pipefile` (a named pipe the engine polls every frame).
  This replaces the separate in-game file watcher DEV_ENV describes. It's simpler and has no polling cost. macOS/Linux only (upstream: nonfunctional on Windows).
- **Test map for the smoke test (B7).** There's no free Q3 map compiler on macOS, and the smoke test needs a map. So this phase adds a small C++ map compiler,
  `tools/mapc`: JSON list of axis-aligned boxes → Q3 BSP (IBSP v46, one leaf, vertex-lit). It produces `maps/testroom.bsp`. P1 extends it.
  This pulls a small slice of P1 forward; nothing else from P1 is done.

## Done when
`./scripts/dev-watch.sh` rebuilds on save and relaunches into `testroom`. Errors show in red in the terminal and as toasts in-game.
`./scripts/smoke.sh` passes, and `dev_crash segv` produces a crash file with a backtrace.
