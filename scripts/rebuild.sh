#!/bin/sh
# Hot rebuild: rebuild only the game modules (qagame, cgame, ui) and reload
# them in the running game without restarting it.
#   ./scripts/rebuild.sh [fast|safe]
# Reload commands (docs/dev-loop.md):
#   map_restart 0 reloads qagame (server game: entities, rules, pmove on the server)
#   vid_restart   reloads cgame and ui (client game, HUD, menus)
# Engine changes (code/client, code/qcommon, renderers...) need a full relaunch.
set -eu
. "$(dirname -- "$0")/lib.sh"

CONFIG=${1:-fast}
[ $# -gt 0 ] && shift
DIR=$(build_dir "$CONFIG")
[ -f "$DIR/CMakeCache.txt" ] || die "no $CONFIG build yet; run ./scripts/build.sh $CONFIG"

START=$(date +%s)
cmake --build "$DIR" --target qagame_base cgame_base ui_base "$@"
printf '%s[%s] modules rebuilt in %ss%s\n' "$C_GRN" "$(now)" "$(( $(date +%s) - START ))" "$C_OFF"

if send_cmd "map_restart 0" && send_cmd "vid_restart"; then
    echo "sent map_restart 0 + vid_restart to the running game"
else
    echo "game not running (or not started by dev.sh). In the console type:  map_restart 0; vid_restart"
fi
