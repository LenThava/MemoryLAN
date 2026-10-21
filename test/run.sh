#!/bin/sh
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d /tmp/mlan-test.XXXXXX)
trap 'rm -f "$tmp/core" "$tmp/quiet" "$tmp/queue" "$tmp/trial" "$tmp/life"; rmdir "$tmp" 2>/dev/null || true' EXIT
cd "$base"
clang++ -std=c++17 -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wno-writable-strings -Wno-deprecated-declarations -Itest/inc -Iesp32 \
  test/core.cpp esp32/io_dev.cpp esp32/util.cpp -o "$tmp/core"
"$tmp/core"
clang++ -std=c++17 -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -DMLAN_TRIAL -Wno-writable-strings -Wno-deprecated-declarations -Itest/inc -Iesp32 \
  test/core.cpp esp32/io_dev.cpp esp32/util.cpp -o "$tmp/quiet"
"$tmp/quiet"
clang++ -std=c++17 -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Itest/inc -Iesp32 test/queue.cpp esp32/io_buf.cpp -o "$tmp/queue"
"$tmp/queue"
clang++ -std=c++17 -O1 -fsanitize=address,undefined -Iesp32 test/trial.cpp -o "$tmp/trial"
"$tmp/trial"
clang++ -std=c++17 -O1 -fsanitize=address,undefined -Itest/life -Iesp32 test/life.cpp -o "$tmp/life"
"$tmp/life"
"$tmp/life" --transfer | PYTHONDONTWRITEBYTECODE=1 python3 test/wire.py
PYTHONDONTWRITEBYTECODE=1 python3 test/lab.py
PYTHONDONTWRITEBYTECODE=1 python3 test/files.py
PYTHONDONTWRITEBYTECODE=1 python3 test/report.py
