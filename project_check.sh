#!/usr/bin/env bash
set -Eeuo pipefail

pass(){ printf "OK   - %s\n" "$*"; }
miss(){ printf "MISS - %s\n" "$*"; }
info(){ printf "INFO - %s\n" "$*"; }
warn(){ printf "WARN - %s\n" "$*"; }

need() { command -v "$1" >/dev/null 2>&1 || { warn "missing tool '$1' (skipping related checks)"; return 1; }; }

has_yml(){ test -n "$(ls .github/workflows/*.yml 2>/dev/null || true)"; }

# 0) Basic build
if cargo check -q; then pass "Repo builds (cargo check)"; else miss "Repo fails to build (cargo check)"; fi

# 1) Protobuf (prost) & versioned messages
grep -q 'prost' Cargo.toml && grep -q 'prost-build' Cargo.toml \
  && pass "prost/prost-build present in Cargo.toml" \
  || miss "prost/prost-build missing in Cargo.toml"

if need rg && rg -n 'CalcRequest|CalcResponse' -S src 2>/dev/null | head -n1 >/dev/null; then
  pass "Versioned messages (CalcRequest/CalcResponse) referenced in src/"
else
  miss "No CalcRequest/CalcResponse references found in src/"
fi

# 2) Frame format: SYNC + type + CRC32
if need rg; then
  sync_hit="$(rg -n '\bSYNC\b' -S src 2>/dev/null | head -n1 || true)"
  crc_hit="$(rg -n 'crc32fast' -S src 2>/dev/null | head -n1 || true)"
  typ_hit="$(rg -n '\bTYPE_(REQ|RESP)\b' -S src 2>/dev/null | head -n1 || true)"
  [ -n "$sync_hit" ] && pass "SYNC constant/usage present ($sync_hit)" || miss "SYNC not found in src/"
  [ -n "$typ_hit"  ] && pass "frame type constants present ($typ_hit)" || miss "TYPE_REQ/TYPE_RESP not found"
  [ -n "$crc_hit"  ] && pass "crc32fast in use ($crc_hit)" || miss "crc32fast not referenced"
fi

# 3) Guard errors & checks
errs_ok=true
for pat in UnknownVersion UnknownType HeaderShort BadSync LengthMismatch Crc Decode; do
  if ! (need rg && rg -n "$pat" -S src 2>/dev/null | head -n1 >/dev/null); then
    errs_ok=false; miss "Guard error missing: $pat"
  fi
done
$errs_ok && pass "Guard error variants present" || true

# 4) compat tests
if [ -f tests/compat.rs ]; then
  pass "tests/compat.rs exists"
  if cargo test --test compat -q; then pass "compat tests pass"; else miss "compat tests failing"; fi
else
  miss "tests/compat.rs missing"
fi

# 5) Legacy ABI
if (need rg && rg -n '^pub\s+fn\s+encode_calc_request_with_trace\s*\(' -S src 2>/dev/null | head -n1 >/dev/null); then
  pass "encode_calc_request_with_trace is present"
else
  miss "encode_calc_request_with_trace missing"
fi

# 6) Focused CRC test (so failures are obvious)
if cargo test --test codec -- crc_mismatch_is_error -q; then
  pass "crc_mismatch_is_error passes"
else
  miss "crc_mismatch_is_error failing (CRC likely verified after decode or wrong slice hashed)"
fi

# 7) CI workflows sanity
if has_yml; then
  pass "GitHub workflows detected"
  # actionlint if available (best)
  if need actionlint; then
    if actionlint -color=never; then pass "actionlint: workflows valid"; else miss "actionlint: workflow issues detected"; fi
  else
    info "Install actionlint for deep workflow checks: 'brew install actionlint' (macOS) or see https://github.com/rhysd/actionlint"
  fi
  # cheap check for missing 'jobs:' in renode-smoke
  for f in .github/workflows/*renode*.yml; do
    [ -e "$f" ] || continue
    if ! grep -qE '^\s*jobs:\s*$' "$f"; then miss "workflow $f is missing a 'jobs:' section"; else pass "$f has jobs:"; fi
  done
else
  miss "No workflows under .github/workflows/"
fi

# 8) cargo-deny config sanity
if [ -f deny.toml ]; then
  pass "deny.toml present"
  val="$(sed -nE 's/^[[:space:]]*unmaintained[[:space:]]*=[[:space:]]*"([^"]+)".*/\1/p' deny.toml | head -n1 || true)"
  case "$val" in
    all|workspace|transitive|none) pass "advisories.unmaintained is valid: \"$val\"" ;;
    "") warn "advisories.unmaintained not found; cargo-deny will default" ;;
    *) miss "advisories.unmaintained invalid: \"$val\" (use all|workspace|transitive|none)";;
  esac
else
  info "deny.toml not present"
fi

# 9) Fuzz harness/toolchain hints
if [ -d fuzz ]; then
  pass "fuzz/ directory present"
  if need cargo-fuzz; then
    info "You can run: cargo fuzz build  (nightly toolchain required)"
  else
    warn "cargo-fuzz not installed. Install: 'cargo install cargo-fuzz' and 'rustup toolchain install nightly'"
  fi
else
  info "No fuzz/ directory; skipping cargo-fuzz checks"
fi

# 10) PR status (requires gh)
if need gh; then
  url="$(git remote get-url origin 2>/dev/null || echo "")"
  REPO="${REPO:-}"
  if [[ -z "$REPO" ]]; then
    case "$url" in
      git@github.com:*) REPO="${url#git@github.com:}"; REPO="${REPO%.git}";;
      https://github.com/*) REPO="${url#https://github.com/}"; REPO="${REPO%.git}";;
      *) REPO="";;
    esac
  fi
  if [ -n "$REPO" ]; then
    for pr in 22 23 24 26; do
      gh pr view "$pr" -R "$REPO" --json number,title,state,mergedAt,url \
        --template '{{.number}} {{printf "%-35s" .title}} {{.state}} {{.mergedAt}} {{.url}}
' 2>/dev/null || true
    done | while read -r num title state merged url; do
      case "$state" in
        MERGED) pass "PR #$num merged: $title" ;;
        OPEN)   info "PR #$num open:   $title" ;;
        *)      info "PR #$num state=$state: $title" ;;
      esac
    done
  else
    info "Could not infer GitHub repo from 'origin'; set REPO=owner/name and re-run."
  fi
else
  info "Skip PR status (GitHub CLI 'gh' not installed)"
fi

# 11) Full test suite (quick signal)
branch="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo '(no git)')"
if cargo test -q; then pass "Tests green on current branch ($branch)"; else miss "Tests failing on current branch ($branch)"; fi

echo "== Checks complete =="
