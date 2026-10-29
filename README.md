<p align="center"><img src="misc/skaar/skaar-mark-gold.png" alt="Skaar" width="140"></p>

# Skaar

An open-source aim trainer and movement shooter. Early development, public, and open to anyone: fork it, mod it, send a PR.

- **Aim trainer**: clicking, tracking and target-switching scenarios. Each one is a JSON
  file, so anyone can add or tune them: see [docs/scenarios.md](docs/scenarios.md).
- **Movement** (planned): strafe jumping, air control and double jumps, as a ruleset
  you pick per mode alongside classic physics.
- **Tactical mode** (planned): a rifle and a pistol, data-driven recoil, spread and
  counter-strafing.

Runs on its own free assets. No Quake III data needed.

## Install and play

macOS (Apple Silicon; Intel should work too) and Linux. Open a terminal and run:

```sh
git clone https://github.com/stackedlol/skaar.git
cd skaar
./install.sh
```

Then play with `skaar` from any terminal. On a Mac, if Apple's Command Line Tools
popup appears, let it finish and run `./install.sh` again. `skaar update` gets the
latest version. Expect rough edges; crash reports land in `logs/` (`skaar crash`).

## Developing

```sh
brew install cmake ninja ccache fswatch
./scripts/build.sh      # -> build-fast/
./scripts/dev.sh        # launch into the test room
```

[Build notes](docs/build-macos.md) · [Scenarios](docs/scenarios.md) · [Dev loop](docs/dev-loop.md) · [Project brief](docs/BRIEF.md) · [Assets](docs/assets.md)

## Acknowledgments

- [ioquake3](https://github.com/ioquake/ioq3): the engine Skaar is forked from ([upstream README](https://github.com/ioquake/ioq3#readme)).
- [id Software](https://github.com/id-Software/Quake-III-Arena): the original Quake III Arena source release.
- Fonts and other third-party assets are credited in [docs/assets.md](docs/assets.md).

## License

GPLv2-or-later. See [LICENSE](LICENSE).
