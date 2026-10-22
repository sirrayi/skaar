# Skaar — project memory (auto-loaded by every Claude Code session)

Fork of ioquake3 (id Tech 3, GPLv2) turned into a standalone aim trainer + arena/movement shooter.
**Every session MUST read `docs/BRIEF.md` §9 "CURRENT SCOPE" before writing any code.** That section overrides all earlier sections where they conflict.

## Language decision (final — do not relitigate)
- **Engine core stays C.** `code/qcommon`, `code/client`, `code/server`, `code/renderergl2`, `code/renderergl1`, `code/renderercommon`, `code/sdl`, `code/sys`, `code/botlib` are a 540-file GPL fork. Do **not** convert engine `.c` → `.cpp`; no behavioral payoff, high regression risk.
- **All new gameplay code is C++20**, in top-level `game/` (`game/sgame` → qagame, `game/cgame` → cgame, `game/ui` → ui, `game/shared`). These are native modules, **not** QVM: standalone builds always load them natively. Upstream `code/game`, `code/cgame`, `code/q3_ui` stay as unbuilt C reference (`BUILD_BASEQ3_MODULES=OFF`). The exception is the movement code in `code/game/bg_*.c`, which is still compiled into sgame and cgame until P5 turns it into the ruleset layer.
- C++ modules must export `extern "C" dllEntry` and `vmMain`, and never throw across that boundary. See `docs/ioq3-layout.md`.

## Legal guardrails (never violate)
- GPLv2-or-later. Keep original copyright headers; source stays public.
- **CPMA mod code/assets are closed source.** Never copy, decompile, or ship them. Implement "CPM-style" movement from documented behavior only (Quake wiki / community physics write-ups).
- Never commit or ship `pak0.pk3` or any Q3 asset. Own/free assets only.
- No trademarked names (Quake, CPMA, Valorant, CS:GO, KovaaK's, Reflex) in the product name or UI. "Inspired by" is fine in internal notes only.

## Product direction
- **Aim trainer** in the spirit of KovaaK's: tracking / flicking / clicking / target-switch scenarios. Scenarios are **data-driven (JSON)** so the public can add and configure them.
- **Tac-shooter feel** (Valorant / CS:GO-inspired): start minimal — **AK-47 + Deagle only**. Weapons are data-driven (recoil patterns, spread, movement-accuracy penalty, counter-strafe).
- **CPM-style movement**: strafe jump, air control, double jump / ramp jump. Selectable per mode (vq3 vs cpm).
- Movement and weapons are **rulesets** (swappable data + code), not forks of the engine.

## Current phase (read `docs/plan-PX.md` for the active one)
- P0 done — rebrand, native modules, placeholder assets.
- P0.5 done — dev loop (`./scripts/dev-watch.sh`), crash reports, smoke scripts.
- P1 **in progress** — own game/cgame/ui module skeleton, boots into an empty test room.
- Next in order: P2 solo lobby → P3 aim trainer v1 → P4 feel pass → P5 CPM movement → P6 CPM duel. Tac weapons (AK-47/Deagle) and surf come after, per BRIEF §9.

## Working rules
- One milestone at a time. Write `docs/plan-PX.md` before coding; after each task report: what runs, how to run it, what's next.
- Keep the macOS (Apple Silicon) build green after every change. Small commits.
- New dependencies only via `docs/deps.md`.

## Parallel sessions
If multiple agents work this repo at once: read this file + BRIEF §9 first, **claim your files in the relevant `docs/plan-PX.md` before editing**, and never touch another agent's claimed files. The `docs/` directory is the shared coordination channel.
