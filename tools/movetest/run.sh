#!/bin/sh
# Build and run the headless movement tests (tools/movetest/movetest.cpp).
#   tools/movetest/run.sh            exit status 0 = all passed
# Builds into build-movetest/ with the game modules' compilers. FP contraction is
# off so the parity test compares the C and C++ code paths, not FMA choices.
set -eu
REPO=$(cd "$(dirname -- "$0")/../.." && pwd)
OUT="$REPO/build-movetest"
mkdir -p "$OUT"

CC=${CC:-cc}
CXX=${CXX:-c++}
FLAGS="-O2 -g -ffp-contract=off -DQAGAME -I$REPO/code -I$REPO/game -I$REPO/game/shared"
JSON="-I$REPO/code/thirdparty/nlohmann-json-3.11.3"

objs=
for src in qcommon/q_math.c qcommon/q_shared.c qcommon/dev_assert.c game/bg_misc.c game/bg_pmove.c game/bg_slidemove.c; do
    obj="$OUT/$(basename "$src" .c).o"
    "$CC" $FLAGS -w -c "$REPO/code/$src" -o "$obj"
    objs="$objs $obj"
done

"$CXX" -std=c++20 $FLAGS $JSON -Wall -Wextra -Werror \
    "$REPO/tools/movetest/movetest.cpp" \
    "$REPO/game/shared/move_pmove.cpp" "$REPO/game/shared/move_slide.cpp" "$REPO/game/shared/move_ruleset.cpp" \
    $objs -o "$OUT/movetest"

exec "$OUT/movetest" "$REPO"
