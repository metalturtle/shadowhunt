#!/usr/bin/env bash
# Local match launcher: rebuilds, stops any previous local match, then starts
# a headless server, windowed clients and optional bots. Ctrl-C stops them all.
#
#   ./play.sh                 server + 2 windowed clients
#   ./play.sh -c 1 -b 3       you plus three bots
#   ./play.sh -b 6 -c 0       watch-free bot match (headless, logs only)
#   ./play.sh --fast          short lobby/release/results timers
#   ./play.sh --level tests/fixtures/stealth_duel.json --tuning my_tuning.json
#   ./play.sh --web -c 0 -b 2  also serve the browser build on http://localhost:8080/
#
# Edit levels/level.json or levels/config/tuning.json while it runs: both hot
# reload (walls excepted).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
EXE="$ROOT/build/Debug/unix_main"
# pkill -f takes a regex; escape the path (it may contain "c++" and the like).
EXE_PATTERN="$(printf '%s' "$EXE" | sed 's/[][\.*^$+?(){}|]/\\&/g')"
CLIENTS=2
BOTS=0
BUILD=1
PORT=8000
WEB=0
LOG_DIR="$ROOT/.play-logs"
EXTRA_ENV=()

usage() { sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'; exit "${1:-0}"; }

while [ $# -gt 0 ]; do
  case "$1" in
    -c|--clients) CLIENTS="$2"; shift 2 ;;
    -b|--bots) BOTS="$2"; shift 2 ;;
    --level) EXTRA_ENV+=("SHADOWHUNT_LEVEL=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"); shift 2 ;;
    --tuning) EXTRA_ENV+=("SHADOWHUNT_TUNING=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"); shift 2 ;;
    --fast) EXTRA_ENV+=("SHADOWHUNT_TUNING=$ROOT/tests/fixtures/fast_tuning.json"); shift ;;
    --port) PORT="$2"; shift 2 ;;
    --web) WEB=1; shift ;;
    --no-build) BUILD=0; shift ;;
    -h|--help) usage 0 ;;
    *) echo "unknown option: $1" >&2; usage 1 ;;
  esac
done

if [ $((CLIENTS + BOTS)) -gt 8 ]; then
  echo "at most 8 players (clients + bots)" >&2; exit 1
fi

if [ "$BUILD" = 1 ]; then
  cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Debug >/dev/null
  cmake --build "$ROOT/build" -j8 --target unix_main | grep -E "error|warning: |Built target unix_main" || true
fi

stop_all() {
  pkill -INT -f "$EXE_PATTERN" 2>/dev/null || true
  sleep 0.5
  pkill -KILL -f "$EXE_PATTERN" 2>/dev/null || true
  if [ -n "${WEB_PID:-}" ]; then kill "$WEB_PID" 2>/dev/null || true; fi
  # Stop the log follower subshell and its tail/grep children.
  if [ -n "${FOLLOW_PID:-}" ]; then
    pkill -P "$FOLLOW_PID" 2>/dev/null || true
    kill "$FOLLOW_PID" 2>/dev/null || true
  fi
}

# A previous match still holding the port would make the new server fail.
stop_all
mkdir -p "$LOG_DIR"
trap 'trap - EXIT; echo; echo "stopping match"; stop_all; exit 0' EXIT INT TERM

cd "$ROOT"
env ${EXTRA_ENV[@]+"${EXTRA_ENV[@]}"} SDL_VIDEODRIVER=dummy SHADOWHUNT_SERVER_PORT="$PORT" \
  "$EXE" > "$LOG_DIR/server.log" 2>&1 &
sleep 0.4

next_port=$((PORT + 1))
for ((i = 1; i <= CLIENTS; i++)); do
  env ${EXTRA_ENV[@]+"${EXTRA_ENV[@]}"} "$EXE" "$next_port" 127.0.0.1 "$PORT" \
    > "$LOG_DIR/client-$i.log" 2>&1 &
  next_port=$((next_port + 1))
done
for ((i = 1; i <= BOTS; i++)); do
  env ${EXTRA_ENV[@]+"${EXTRA_ENV[@]}"} SDL_VIDEODRIVER=dummy SHADOWHUNT_HEADLESS=1 SHADOWHUNT_BOT=1 \
    "$EXE" "$next_port" 127.0.0.1 "$PORT" > "$LOG_DIR/bot-$i.log" 2>&1 &
  next_port=$((next_port + 1))
done

if [ "$WEB" = 1 ]; then
  if [ ! -f "$ROOT/build-web/web/shadowhunt.html" ]; then
    echo "no browser build: see README (emcmake cmake -S . -B build-web && cmake --build build-web)" >&2
    exit 1
  fi
  node "$ROOT/tools/web_server.js" --dir "$ROOT/build-web/web" --game-port "$PORT" \
    > "$LOG_DIR/web.log" 2>&1 &
  WEB_PID=$!
  echo "browser build on http://localhost:8080/ (relay log: .play-logs/web.log)"
fi

echo "server on UDP $PORT, $CLIENTS client(s), $BOTS bot(s); logs in .play-logs/"
echo "following the server log (Ctrl-C stops everything)"
# Follow in the background and wait: bash only runs the cleanup trap between
# commands, and `wait` is interruptible where a foreground pipeline is not.
( tail -n +1 -f "$LOG_DIR/server.log" | grep --line-buffered -E \
  "round [0-9]+:|match state|winner|eliminated|tagged|pellet taken|hot reload|tuning|adding client|disconnecting" ) &
FOLLOW_PID=$!
wait "$FOLLOW_PID"
