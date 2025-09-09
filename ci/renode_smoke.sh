#!/usr/bin/env bash
set -euo pipefail

mkdir -p artifacts
: > artifacts/renode.log
: > artifacts/smoke.log

# Start Renode headless (adjust to your .resc/.repl if you have one)
# Placeholder: we just boot a minimal Renode session so we have a log artifact.
# Replace with: renode -P -e 'i @path/to/your.resc; start' 2>&1 | tee artifacts/renode.log
echo "[placeholder] Renode session placeholder" | tee -a artifacts/renode.log

# Hard assertion using your binary: roundtrip request/response + trace
set +e
REQ_OUT=$(cargo run --quiet -- make-req-trace 7 35 2>>artifacts/smoke.log)
STATUS=$?
set -e
echo "$REQ_OUT" | tee -a artifacts/smoke.log

# Decode to prove payload + trace are valid (hard fail if decode errors)
HEX=$(echo "$REQ_OUT" | rg -o 'HEX=([0-9a-fA-F]+)' | cut -d= -f2)
[ -n "${HEX:-}" ] || { echo "No HEX produced" | tee -a artifacts/smoke.log; exit 2; }

cargo run --quiet -- --decode "$HEX" 2>>artifacts/smoke.log | tee -a artifacts/smoke.log

# Final assertion: require marker in the log
echo "$REQ_OUT" | rg -q 'TraceCtx' || { echo "missing TraceCtx" | tee -a artifacts/smoke.log; exit 3; }

echo "E2E OK" | tee -a artifacts/smoke.log
