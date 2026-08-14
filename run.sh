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

if [ ! -d build ]; then
  cmake -B build -DCMAKE_BUILD_TYPE=Release
fi
cmake --build build -j

case "${1:-web}" in
  web)      exec ./build/dpe web ;;
  test)     exec ./build/dpe_tests ;;
  rank)     shift || true; exec ./build/dpe rank "$@" ;;
  detail)   shift || true; exec ./build/dpe detail "$@" ;;
  dump)     shift || true; exec ./build/dpe dump "$@" ;;
  markets)  shift || true; exec ./build/dpe markets "$@" ;;
  categories) shift || true; exec ./build/dpe categories "$@" ;;
  *) echo "Usage: $0 {web|rank|detail <id>|dump|markets|categories|test}"; exit 2 ;;
esac
