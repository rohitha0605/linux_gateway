set -euo pipefail
REQ_HEX="$(cargo run --quiet -- make-req-trace 7 35 | tail -n1)"
RESP_HEX="$(cargo run --quiet -- rpmsg-bounce "$REQ_HEX" | tail -n1)"

REQ_TID=$(cargo run --quiet --example decode_req  -- "$REQ_HEX"  | awk -F'"' '/TRACE_ID=Some/ {print $2}')
RESP_TID=$(cargo run --quiet --example decode_resp -- "$RESP_HEX" | awk -F'"' '/TRACE_ID=Some/ {print $2}')

echo "REQ_TID=$REQ_TID"
echo "RESP_TID=$RESP_TID"
test "$REQ_TID" = "$RESP_TID"
echo "OK: same trace id"
