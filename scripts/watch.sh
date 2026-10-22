#!/bin/sh
# Live error watcher: follows logs/latest.log (across relaunches) and shows only
# problems. Red: ERROR, FATAL, ASSERT, sanitizer reports, crashes. Yellow:
# WARNING, frame spikes. Each line gets the time it was seen; file:line
# references are highlighted when present. Bursts of the same message (e.g.
# 100 missing sounds) show the first 3, then a "+N more like this" summary;
# counts reset when a new session launches.
#   ./scripts/watch.sh            filtered view
#   ./scripts/watch.sh --all      everything, with the same coloring
set -eu
. "$(dirname -- "$0")/lib.sh"

ALL=0
[ "${1:-}" = --all ] && ALL=1

mkdir -p "$LOGS"
[ -e "$LOGS/latest.log" ] || : > "$LOGS/latest.log"

printf '%swatching %s (Ctrl+C to stop)%s\n' "$C_DIM" "$LOGS/latest.log" "$C_OFF"

# tail -F follows the latest.log symlink when dev.sh points it at a new session
tail -n 0 -F "$LOGS/latest.log" 2>/dev/null | awk -v all="$ALL" \
    -v red="$C_RED" -v yel="$C_YEL" -v dim="$C_DIM" -v off="$C_OFF" '
{
    line = $0
    gsub(/\^[0-9A-Za-z]/, "", line)        # Q3 color codes
    gsub(/\033\[[0-9;]*m/, "", line)       # ANSI colors from upstream output

    color = ""
    if (line ~ /ERROR|^Error:|FATAL|ASSERT|Sanitizer|runtime error:|SIGSEGV|SIGABRT|SIGBUS|Segmentation fault|crash report/)
        color = red
    else if (line ~ /WARNING|spike/)
        color = yel
    else if (!all)
        next

    if (line ~ /launching (fast|safe) build/) {
        flush_suppressed()
        delete seen
    }

    # same message with a different path/number = same kind of problem
    key = line
    gsub(/[^ ]*\/[^ ]*/, "<path>", key)
    gsub(/[0-9]+/, "#", key)
    if (++seen[key] > 3) {
        if (!(key in suppressed)) order[++norder] = key
        suppressed[key]++
        next
    }
    flush_suppressed()

    if (match(line, /[A-Za-z0-9_\/.+-]+\.(c|cc|cpp|h|hpp|m|mm|cfg|json):[0-9]+/))
        line = substr(line, 1, RSTART - 1) "\033[4m" substr(line, RSTART, RLENGTH) "\033[24m" substr(line, RSTART + RLENGTH)

    printf "%s[%s]%s %s%s%s\n", dim, clock(), off, color, line, (color != "" ? off : "")
    fflush()
}

function clock(   cmd, t) {
    cmd = "date +%H:%M:%S"; cmd | getline t; close(cmd)
    return t
}

function flush_suppressed(   i, k) {
    for (i = 1; i <= norder; i++) {
        k = order[i]
        printf "%s[%s]   (+%d more like: %s)%s\n", dim, clock(), suppressed[k], k, off
        delete suppressed[k]
    }
    norder = 0
    fflush()
}'
