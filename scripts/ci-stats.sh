#!/usr/bin/env bash
set -euo pipefail
RID=$(gh run list --branch main --workflow CI -L 1 --json databaseId -q '.[0].databaseId' || true)
# Prefer workflow_run summaries; fall back to manual dispatch
SID=$(gh run list --workflow ".github/workflows/ci-summary.yml" --event workflow_run -L 1 --json databaseId -q '.[0].databaseId' || true)
if [[ -z "${SID:-}" ]]; then
  SID=$(gh run list --workflow ".github/workflows/ci-summary.yml" --event workflow_dispatch -L 1 --json databaseId -q '.[0].databaseId' || true)
fi
echo "CI:  ${RID:-<none>}"
echo "SUM: ${SID:-<none>}"
if [[ -n "${SID:-}" ]]; then
  gh run view "$SID" --log | grep -E "JUnit:|Line coverage:"
fi
