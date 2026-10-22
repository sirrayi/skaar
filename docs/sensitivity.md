# Sensitivity

Skaar turns `sensitivity × m_yaw` degrees per mouse count (`m_yaw` 0.022 by default), with raw input and no
acceleration (docs/feel.md). The math is in `game/shared/sens.h` and checked in `tests/test_sens.cpp`.

- **cm per 360°** = 360 / (sensitivity × yaw) / dpi × 2.54. Settings › Aim shows it, and you can type a cm/360 to set it.
- **Converting from another game:** keep sensitivity × yaw the same. Ours = theirs × their yaw / 0.022.
  Pick their yaw in Settings › Aim › Convert from, type their sensitivity, Apply.

| Yaw | Typical for |
|---|---|
| 0.022 | Most games descended from the classic arena shooters, and their engine family |
| 0.07 | One popular tactical shooter (a fixed value) |
| 0.0066 | Several modern hero and military shooters |

The yaw values are the ones community converters agree on, checked 2026-10-22 against
[csdb.gg](https://csdb.gg/sensitivity-converter/) and
[recharge.com](https://www.recharge.com/blog/en-gb/valorant-sensitivity-converter-cs2-apex-ow2-fortnite).
Games whose sensitivity depends on FOV aren't listed, because a yaw-only conversion is wrong for them.
Check a conversion with `sens_test` (docs/dev-loop.md): turn 360° and compare the reported counts.
