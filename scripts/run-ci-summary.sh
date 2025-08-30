#!/usr/bin/env bash
set -euo pipefail

# 1) Latest successful CI run on main
RID=$(gh run list --branch main --workflow CI -L 1 --json databaseId --jq '.[0].databaseId')
echo "CI RID=$RID"

# 2) Manually trigger the CI Summary for that run (requires workflow_dispatch in ci-summary.yml)
gh workflow run ".github/workflows/ci-summary.yml" -f run_id="$RID"

# 3) Give GitHub a moment to create the run, then grab its ID
sleep 6
SID=$(gh run list --workflow ".github/workflows/ci-summary.yml" --event workflow_dispatch -L 1 --json databaseId --jq '.[0].databaseId')
echo "Summary SID=$SID"

# 4) Wait for it and print the numbers
gh run watch "$SID"
gh run view "$SID" --log | grep -E 'JUnit:|Line coverage:'
