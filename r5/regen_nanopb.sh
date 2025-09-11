#!/usr/bin/env bash
set -euo pipefail
: "${NANOPB:=third_party/nanopb}"   # set env if your path differs

PROTO=proto/rpmsg/calc/v1/calc.proto
OUTDIR=r5/gen

mkdir -p "$OUTDIR"
protoc \
  -I proto \
  --plugin=protoc-gen-nanopb="$NANOPB/generator/protoc-gen-nanopb" \
  --nanopb_opt_file=proto/nanopb.options \
  --nanopb_out="$OUTDIR" \
  "$PROTO"

echo "Generated: $OUTDIR/$(basename "$PROTO" .proto).pb.[ch]"
