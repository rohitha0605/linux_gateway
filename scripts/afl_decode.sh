#!/usr/bin/env bash
set -euo pipefail
HEX="$(xxd -p -c 999999 "$1" | tr -d '\n\r\t ')"
./target/debug/linux_gateway --decode="$HEX" >/dev/null 2>&1 || true
./target/debug/linux_gateway --decode="0x$HEX" >/dev/null 2>&1 || true
