# Dev environment and live error visibility (macOS first)

Goal: while the human plays and tests, every error, warning, assert, crash and frame spike is visible immediately and saved to a log. Nothing fails silently. Set this up BEFORE writing any gameplay code (it is task P0.5, right after the P0 build works).

## Part A: what the human does once (macOS)
1. Install tools: `xcode-select --install`, then `brew install cmake ninja sdl2 git ccache fswatch`.
2. Confirm: `clang --version`, `lldb --version`, `cmake --version` all print.
3. Give the agent a clean repo folder and tell it to follow this file.
4. When testing, use ONE command: `./scripts/dev-watch.sh`. It rebuilds and relaunches the game every time you or the agent save a file, like a web dev server. (If you want a second window for filtered errors only, run `./scripts/watch.sh`.)
5. If the game crashes, paste the newest file from `logs/crash-*.txt` to the agent. Do not retype the error.

## Part B: agent tasks (do in order, each must work before the next)

### B1. Two build configs (scripts/build.sh with a flag)
- `./scripts/build.sh fast`: optimized with debug symbols (`-O2 -g`). Use for judging feel and framerate.
- `./scripts/build.sh safe`: `-O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer`. Use for hunting memory bugs and undefined behavior. Slower, not for feel testing.
- Both write to separate build dirs (`build-fast/`, `build-safe/`). Warnings on: `-Wall -Wextra`. Treat new warnings in our own code as errors (`-Werror` for `/game` and `/tools`, not for upstream ioq3 code).
- Use ccache so rebuilds are fast.

### B2. Run script with logging (scripts/dev.sh)
- Launches the game binary with the dev cvars: `+set developer 1 +set logfile 2 +set com_hunkMegs 256` (verify each cvar exists in the ioq3 version; document in /docs).
- Pipes stdout and stderr through `tee` into `logs/session-YYYYMMDD-HHMMSS.log` and updates a symlink `logs/latest.log`.
- Runs under `lldb` in batch mode so a crash auto-prints a full backtrace into the log: `lldb -b -o run -k "bt all" -k "quit" -- <binary> <args>`.
- Supports `./scripts/dev.sh safe` to run the sanitizer build instead.

### B3. Live error watcher (scripts/watch.sh)
- `tail -F logs/latest.log` filtered with grep for: `ERROR|WARNING|ASSERT|FATAL|Sanitizer|SIGSEGV|SIGABRT|spike`.
- Color: red for ERROR/FATAL/ASSERT/Sanitizer, yellow for WARNING/spike. Prints the time and the file:line when present.

### B4. In-game error overlay
- Any `Com_Error`, `Com_Printf` of a warning, or failed assert shows a short on-screen toast in the top-left for 5 seconds with the message and file:line (does not pause the game).
- Fatal errors show a full-screen message with the log path, not a silent exit.
- A cvar `dev_overlay 1` toggles an always-on corner panel: FPS, frame time (ms), tick rate, ping to local server, entity count, last 3 warnings.

### B5. Soft asserts and crash handler
- Macro `DEV_ASSERT(cond, "message")` in our code: logs `ASSERT file:line message` + backtrace, shows the toast, and keeps running in dev builds. In release builds it compiles to a no-op.
- Install a signal handler for SIGSEGV, SIGABRT, SIGBUS, SIGFPE that writes `logs/crash-<timestamp>.txt` containing: signal, backtrace (`backtrace_symbols_fd`), the last 200 lines of the log, the current map, the active ruleset/mode, and the OS/CPU/GPU info. Then exits cleanly.

### B6. Frame and input diagnostics (this is what makes it feel good)
- Log a `spike` line whenever a frame exceeds 2x the running average (include ms and what ran that frame).
- Log a warning if the fixed-timestep loop falls behind (accumulator > N ticks).
- Dev command `sens_test`: prints measured counts per full turn so cm/360 can be verified against the converter.

### B7. Smoke test before each commit (scripts/smoke.sh)
- Builds `safe`, starts a dedicated server headless (`+set dedicated 1 +map <testmap>`), runs for 20 seconds, exits.
- Fails (non-zero exit) if the log contains ERROR, ASSERT, or any sanitizer report.
- The agent must run this and report the result before saying a task is done.

### B8. Hot rebuild loop
- `./scripts/rebuild.sh` rebuilds only the game/cgame/ui native modules and tells the human to type `vid_restart` or `map <name>` in the console to reload (verify what actually reloads the module in this ioq3 version and write it in /docs/dev-loop.md).

### B9. "Localhost" style auto-rebuild loop (scripts/dev-watch.sh)
This is the game version of a web dev server: save a file, it rebuilds, relaunches, and shows errors. One command, one terminal.
- Uses `fswatch` to watch `game/`, `engine/`, `sim/`, `data/` for changes (debounce 500 ms).
- On change: run `./scripts/build.sh fast`.
  - If the build FAILS: do not relaunch. Print the compiler errors in red at the bottom of the terminal, and keep the last working game running.
  - If the build SUCCEEDS: close the running game and relaunch it with `./scripts/dev.sh`, auto-loading the last map and mode (`+map <lastmap>`, stored in `logs/last-session.cfg`).
- For data-only changes (scenario JSON, configs, maps): skip the rebuild and send a reload command to the running game instead (add a console command `dev_reload` that re-reads scenarios and configs, and a tiny file watcher inside the game that triggers it).
- Prints a one-line status after each cycle: `[12:04:11] build ok 3.2s, relaunched` or `[12:04:11] build FAILED, 2 errors`.
- All output also goes to `logs/latest.log`, so `watch.sh` still works.
- Stop with Ctrl+C, which also closes the game.
- Document in /docs/dev-loop.md: how to start it, what it watches, and known limits (a full relaunch is needed after engine C changes).

## Part C: rules for the agent
- Never mark a task done until `./scripts/smoke.sh` passes and the human has a one-line "how to see it" instruction.
- Any new feature must log its key events at `developer 1` level so the watcher shows what happened.
- If a macOS-specific issue appears (OpenGL deprecation warnings, SDL2 linking, code signing/Gatekeeper), write the fix in /docs/macos-notes.md so the Ubuntu setup later can be compared.
- Keep scripts POSIX-friendly so they run unchanged on Ubuntu.

## Part D: Ubuntu later (human)
1. `sudo apt install build-essential cmake ninja-build libsdl2-dev git ccache gdb`
2. Same scripts; `lldb` lines switch to `gdb -batch -ex run -ex "bt full"` (agent adds this switch).
3. NVIDIA proprietary driver installed (needed for OpenGL/Vulkan performance on the 1080 Ti).
