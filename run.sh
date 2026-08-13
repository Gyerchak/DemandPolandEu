#!/usr/bin/env bash
# Launch the DemandPolandEu monitor.
#   ./run.sh rank        -> CLI ranking table
#   ./run.sh web         -> web dashboard (http://127.0.0.1:8000)
#   ./run.sh detail <id> -> one product detail
#   ./run.sh test        -> run unit tests
set -euo pipefail
cd "$(dirname "$0")"

case "${1:-rank}" in
  web)   exec python3 -m market.web ;;
  test)  exec python3 -m unittest discover -s tests -v ;;
  rank)  shift || true; exec python3 -m market.cli rank "$@" ;;
  detail) shift || true; exec python3 -m market.cli detail "$@" ;;
  dump)  shift || true; exec python3 -m market.cli dump "$@" ;;
  *)     echo "Usage: $0 {rank|web|detail <id>|dump|test}"; exit 2 ;;
esac
