#!/usr/bin/env bash
set -euo pipefail

mkdir -p _artifacts

pack() {
  local label="$1"; shift
  local any=0
  for p in "$@"; do
    if compgen -G "$p" >/dev/null 2>&1; then any=1; break; fi
  done
  if (( any )); then
    tar -czf "_artifacts/${label}.tgz" \
      --ignore-failed-read --numeric-owner --owner=0 --group=0 "$@" || true
  fi
}

# These globs are fine even if directories don't exist; we guard above.
pack fuzz_artifacts fuzz/artifacts/* || true
pack fuzz_corpus    fuzz/corpus/*    || true
pack fuzz_logs      fuzz/*/crashes fuzz/*/queue fuzz/findings/* fuzz/logs/* || true

ls -l _artifacts || true
