#!/usr/bin/env bash
# Build and run the DemandPolandEu trade monitor (C++).
#   ./run.sh             -> web dashboard (http://127.0.0.1:8000)
#   ./run.sh rank        -> CLI import/export ranking
#   ./run.sh detail <id> -> one product detail
#   ./run.sh dump        -> full JSON result
#   ./run.sh markets     -> list markets
#   ./run.sh categories  -> list categories
#   ./run.sh test        -> run unit tests
set -euo pipefail
cd "$(dirname "$0")"
HOST_ARGS=()
SRV=""
cleanup() { [ -n "$SRV" ] && kill "$SRV" 2>/dev/null || true; }
trap cleanup EXIT INT TERM
while [[ $# -gt 1 ]]; do
  case "$1" in
    --host) HOST_ARGS+=("--host" "$2"); shift 2 ;;
    --port) export PORT="$2"; HOST_ARGS+=("--port" "$2"); shift 2 ;;
    *) break ;;
  esac
done

if [ ! -d build ]; then
  cmake -B build -DCMAKE_BUILD_TYPE=Release
fi
cmake --build build -j

case "${1:-web}" in
  web)
    URL="http://127.0.0.1:${PORT:-8000}"
    echo "  DemandPolandEu dashboard: $URL"
    # kill any STALE dpe server on this port first (old binary answering
    # = you'd see outdated UI; port-busy would silently fail the new one)
    # NOTE: `|| true` is REQUIRED — with `set -o pipefail`, grep returning
    # "no match" (exit 1) would otherwise abort the whole script via set -e.
    STALE="$(ss -tlnp 2>/dev/null | grep "127.0.0.1:${PORT:-8000}" | grep -oP 'pid=\K[0-9]+' | head -1 || true)"
    if [ -n "$STALE" ]; then
      echo "  stale server found on port ${PORT:-8000} (pid $STALE) — restarting fresh"
      kill "$STALE" 2>/dev/null || true; sleep 0.5
    fi
    ./build/dpe web "${HOST_ARGS[@]}" &
    SRV=$!
    # wait until the server actually answers (max ~10s), then open a NEW
    # browser tab the moment it's up — no fixed sleep, no race.
    for i in $(seq 1 50); do
      if command -v curl >/dev/null 2>&1; then
        [ -n "$(curl -s -o /dev/null -w '%{http_code}' "$URL/" 2>/dev/null)" ] && break
      else
        (exec 3<>"/dev/tcp/127.0.0.1/${PORT:-8000}") 2>/dev/null && break
      fi
      sleep 0.2
    done
    # Open in an 80% x 80% window (centered) — percentage of the real screen,
    # not a fixed size. Still closable normally (window controls / Alt+F4).
    open_dashboard() {  # $1 = browser bin; rest = flags
      local bin="$1"; shift
      "$bin" "$@" >/dev/null 2>&1 || xdg-open "$URL" >/dev/null 2>&1 || true
    }
    # find the real browser binary (xdg name != binary name: brave-browser.desktop -> brave)
    BROWSER_BIN=""
    for cand in brave brave-browser brave-browser-stable google-chrome google-chrome-stable chromium chromium-browser microsoft-edge firefox; do
      if command -v "$cand" >/dev/null 2>&1; then BROWSER_BIN="$cand"; break; fi
    done
    # screen size (X11: xrandr primary "*" line; fallback: compute from kstart-less math)
    SCREEN_W=1920; SCREEN_H=1080
    if command -v xrandr >/dev/null 2>&1; then
      read -r SW SH _ < <(xrandr --current 2>/dev/null | awk '/\*/{print $1; exit}' | tr 'x' ' ')
      [ -n "${SW:-}" ] && [ -n "${SH:-}" ] && { SCREEN_W="$SW"; SCREEN_H="$SH"; }
    fi
    W=$(( SCREEN_W * 8 / 10 )); H=$(( SCREEN_H * 8 / 10 ))
    X=$(( (SCREEN_W - W) / 2 )); Y=$(( (SCREEN_H - H) / 2 ))
    if [ -n "$BROWSER_BIN" ]; then
      # Chromium-family honours --window-size/--window-position; others fall back to xdg-open
      case "$BROWSER_BIN" in
        brave*|chrom*|*chrome*|edge|firefox)
          open_dashboard "$BROWSER_BIN" --window-size="${W},${H}" --window-position="${X},${Y}" "$URL" ;;
        *) open_dashboard "$BROWSER_BIN" "$URL" ;;
      esac
    else
      xdg-open "$URL" >/dev/null 2>&1 || true
    fi
    echo "  dashboard open at 80% window: $URL (close with normal window controls)"
    # keep running until the server stops; Ctrl+C kills both
    wait "$SRV" ;;
  web-noopen) exec ./build/dpe web ;;
  test)     exec ./build/dpe_tests ;;
  rank)     shift || true; exec ./build/dpe rank "$@" ;;
  detail)   shift || true; exec ./build/dpe detail "$@" ;;
  dump)     shift || true; exec ./build/dpe dump "$@" ;;
  markets)  shift || true; exec ./build/dpe markets "$@" ;;
  categories) shift || true; exec ./build/dpe categories "$@" ;;
  *) echo "Usage: $0 {web|rank|detail <id>|dump|markets|categories|test}"; exit 2 ;;
esac
