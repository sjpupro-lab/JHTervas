#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
./scripts/run_tests.sh
./scripts/build_dist.sh
python3 ./scripts/make_hashes.py
echo "[release] done"
