# ioq3 layout and module loading (research notes)

Upstream: https://github.com/ioquake/ioq3 at `87a99d18` (2026-10-12), GPLv2. Remote `upstream`.

## Source tree (`code/`)
| Dir | What | Built into |
|---|---|---|
| `qcommon/` | Core: cvars, commands, filesystem, net, VM host, collision (`cm_*`), `q_shared`/`q_math` | client + server |
| `client/` | Client frame loop, console, input, sound mixer, cgame/ui hosts (`cl_cgame.c`, `cl_ui.c`) | `skaar` |
| `server/` | Server frame loop, snapshots, game module host (`sv_game.c`) | both |
| `sdl/` | SDL2 window, GL context, input (`sdl_input.c`: raw mouse via SDL relative mode), audio | client |
| `sys/` | Platform main, dlopen, PID file, dialogs (`sys_osx.m`) | both |
| `renderergl2/` | OpenGL2 renderer (GLSL, FBO, HDR), loaded as `renderer_opengl2.dylib` | dylib |
| `renderergl1/` | Fixed-function renderer (fallback) | dylib |
| `renderercommon/` | Shared image loaders, fonts, `tr_public.h` interface | both renderers |
| `game/` | Server game module `qagame`: entities, combat, bots (`ai_*`), plus shared `bg_*` (pmove, items) | `base/qagame.dylib` |
| `cgame/` | Client game module: prediction, HUD, effects, view | `base/cgame.dylib` |
| `q3_ui/` | Q3 menu UI module (the one baseq3 uses) | `base/ui.dylib` |
| `ui/` | Team Arena script-driven UI (menudef); only the syscall stubs are used by baseq3 | missionpack only |
| `botlib/` | Bot AAS navigation | client + server |
| `tools/` | lcc/q3asm (QVM toolchain) | off in Skaar |

Shared movement: `game/bg_pmove.c` + `bg_slidemove.c` are compiled into BOTH qagame and cgame, so client prediction
and server sim run identical code. A movement ruleset must live there (or in a shared C++ lib linked into both).

## Build system
CMake only (the old Makefile is gone). Entry: `CMakeLists.txt` includes `cmake/*.cmake`.
Identity lives in `cmake/identity.cmake`; game module source lists live in `cmake/basegame.cmake`.
SDL2: bundled universal dylib on macOS/Windows (`USE_INTERNAL_SDL`), `find_package(SDL2)` on Linux.

## Native module loading
- The VM host is `qcommon/vm.c` `VM_Create(name, syscalls, interpret)`. The `interpret` value comes from the cvars `vm_game`, `vm_cgame`, `vm_ui`:
  `0` = native library, `1` = QVM interpreted, `2` = QVM compiled (JIT).
- With `0`, `FS_FindVM` searches each game dir for `<name>.dylib` (`.so` on Linux, `.dll` on Windows) and `Sys_LoadGameDll` dlopens it, looking up `dllEntry`
  (receives the syscall function pointer) and `vmMain` (the engine calls this with a command number + args).
- With `2`, only `vm/<name>.qvm` is searched, which is why stock ioq3 fails to start without QVMs.
- When `STANDALONE` is defined, Skaar defaults all three to `0` and `VM_Create` forces native loading, so stale archived values like `vm_game 2` can't break it (`vm.c`).
- Module ABI: the syscall numbers and `vmMain` commands in `game/g_public.h`, `cgame/cg_public.h`, `ui/ui_public.h`. A C++ module
  must export `extern "C" dllEntry` and `vmMain` with the same signatures, and never throw across that boundary.

## Reloading native modules (verified in P0.5, details in docs/dev-loop.md)
- qagame: `map_restart 0` (`VM_Restart` dlcloses and dlopens again) or any `map`/`devmap` (`SV_SpawnServer`).
- cgame: `vid_restart`, or any map load/reconnect (`CL_InitCGame`).
- ui: `vid_restart`.
- Rebuilt dylibs are picked up: the engine dlcloses on unload, and the linker replaces the file rather than rewriting it in place.
