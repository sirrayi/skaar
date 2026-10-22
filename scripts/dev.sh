#!/bin/sh
# Launch the client for testing.  ./scripts/dev.sh [fast|safe] [engine args...]
#
# - dev cvars: developer 1, logfile 2 (flushed qconsole.log), com_hunkMegs 256,
#   com_pipefile (lets dev-watch.sh send commands), dev_logdir/dev_logpath
# - all output -> logs/session-YYYYMMDD-HHMMSS.log, logs/latest.log -> newest
# - runs under lldb (macOS) / gdb (Linux) in batch mode: a crash prints a full
#   backtrace into the log, then the engine's own handler writes logs/crash-*.txt.
#   DEV_DEBUGGER=none runs without a debugger.
# - with no +map/+devmap argument, reloads the map from logs/last-session.cfg
set -eu
. "$(dirname -- "$0")/lib.sh"

CONFIG=fast
case "${1:-}" in fast|safe) CONFIG=$1; shift ;; esac

BIN=$(client_bin "$CONFIG")
[ -x "$BIN" ] || die "no $CONFIG build at $BIN; run ./scripts/build.sh $CONFIG"

mkdir -p "$LOGS"
STAMP=$(date +%Y%m%d-%H%M%S)
LOG="$LOGS/session-$STAMP.log"
: > "$LOG"
ln -sf "session-$STAMP.log" "$LOGS/latest.log"

case " $* " in
    *" +map "*|*" +devmap "*) MAPARGS= ;;
    *)
        if [ -f "$LOGS/last-session.cfg" ]; then
            MAPARGS=$(sed -e '/^[[:space:]]*$/d' -e 's/^/+/' "$LOGS/last-session.cfg" | tr '\n' ' ')
        elif [ -f "$(out_dir "$CONFIG")/base/generated.pk3dir/maps/testroom.bsp" ]; then
            MAPARGS="+devmap testroom"
        else
            MAPARGS=
        fi
        ;;
esac

# shellcheck disable=SC2046,SC2086
set -- $(dev_engine_args "$CONFIG") \
    +set developer 1 +set logfile 2 +set com_hunkMegs 256 \
    +set com_pipefile "$PIPE_NAME" +set dev_logpath "$LOG" \
    "$@" $MAPARGS

printf '%s[%s] launching %s build: %s%s\n' "$C_DIM" "$(now)" "$CONFIG" "$*" "$C_OFF" | tee -a "$LOG"

if [ "$CONFIG" = safe ]; then
    # keep running after UBSan reports; ASan still stops on the first memory error
    export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1}"
    export ASAN_OPTIONS="${ASAN_OPTIONS:-abort_on_error=1}"
fi

# An lldb older than the OS can crash while loading system libraries
# (docs/macos-notes.md), so probe it before trusting it with the session.
lldb_works() { lldb -b -o "target create /usr/bin/true" >/dev/null 2>&1; }

DEBUGGER=${DEV_DEBUGGER:-auto}
if [ "$DEBUGGER" = auto ]; then
    if [ "$OS" = Darwin ] && command -v lldb >/dev/null 2>&1; then
        if lldb_works; then
            DEBUGGER=lldb
        else
            printf '%sWARNING: lldb crashes on this macOS (update the Command Line Tools, see docs/macos-notes.md); running without a debugger. Crash reports still go to logs/crash-*.txt%s\n' "$C_YEL" "$C_OFF" | tee -a "$LOG"
            DEBUGGER=none
        fi
    elif command -v gdb >/dev/null 2>&1; then DEBUGGER=gdb
    else DEBUGGER=none
    fi
fi

case "$DEBUGGER" in
    lldb)
        # Let crashes reach the process as signals (so the engine's crash handler
        # runs after lldb prints the backtrace); don't stop on shutdown signals.
        lldb -b \
            -o "settings set platform.plugin.darwin.ignored-exceptions EXC_BAD_ACCESS|EXC_BAD_INSTRUCTION|EXC_ARITHMETIC" \
            -o "process handle SIGTERM SIGINT SIGHUP SIGPIPE SIGUSR1 SIGUSR2 -s false -n false -p true" \
            -o run \
            -k "bt all" -k "continue" \
            -- "$BIN" "$@" 2>&1 | tee -a "$LOG"
        ;;
    gdb)
        gdb -q -batch \
            -ex "handle SIGTERM SIGINT SIGHUP SIGPIPE nostop noprint pass" \
            -ex run -ex "thread apply all bt full" -ex continue \
            --args "$BIN" "$@" 2>&1 | tee -a "$LOG"
        ;;
    none)
        "$BIN" "$@" 2>&1 | tee -a "$LOG"
        ;;
    *) die "unknown DEV_DEBUGGER '$DEBUGGER' (lldb, gdb or none)" ;;
esac
