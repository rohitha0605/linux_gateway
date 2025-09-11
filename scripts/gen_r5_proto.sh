#!/usr/bin/env bash
set -euo pipefail
PROTO=proto/calc.proto
OUTC=r5/firmware/src/generated
OUTH=r5/firmware/include/generated
mkdir -p "$OUTC" "$OUTH"

# Try nanopb Python generator first; fallback to protoc plugin if available
if command -v nanopb_generator >/dev/null 2>&1; then
  nanopb_generator -Iproto -D"$OUTC" "$PROTO"
  # move headers
  find "$OUTC" -name "*.pb.h" -maxdepth 1 -exec mv {} "$OUTH"/ \;
else
  # protoc plugin path hint (adjust if needed)
  NANOPB_PLUG="$(command -v protoc-gen-nanopb || true)"
  if [ -z "${NANOPB_PLUG}" ]; then
    echo "ERROR: nanopb generator not found. Install one of:
      - pip install nanopb (gives 'nanopb_generator')
      - get protoc-gen-nanopb and put it in PATH" >&2
    exit 2
  fi
  protoc -Iproto --nanopb_out="$OUTC" "$PROTO"
  # move headers
  find "$OUTC" -name "*.pb.h" -maxdepth 1 -exec mv {} "$OUTH"/ \;
fi

echo "Generated:"
ls -1 "$OUTC"/*.pb.c "$OUTH"/*.pb.h
