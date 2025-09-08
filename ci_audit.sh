#!/usr/bin/env bash
set -euo pipefail

cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)"

pass(){ printf "OK   - %s\n" "$1"; }
miss(){ printf "MISS - %s\n" "$1"; }
info(){ printf "INFO - %s\n" "$1"; }

has_yml(){ [ -d .github/workflows ] && ls .github/workflows/*.yml >/dev/null 2>&1; }

echo "== CI / Project audit =="

# 1) Renode smoke test in CI + artifact upload
if has_yml && grep -Rqi 'renode' .github/workflows/*.yml 2>/dev/null; then
  pass "Renode referenced in CI workflows"
else
  miss "Renode smoke test workflow"
fi
if has_yml && grep -Rqi 'upload-artifact' .github/workflows/*.yml 2>/dev/null; then
  pass "Artifact upload step present (e.g. logs on failure)"
else
  miss "Upload artifacts from CI (logs/crashes)"
fi

# 2) Fuzzing: libFuzzer/AFL harnesses, corpus, dict, sanitizers, CI jobs, crash artifacts
if [ -f fuzz/Cargo.toml ]; then pass "cargo-fuzz (libFuzzer) harness present"; else miss "cargo-fuzz harness"; fi
if [ -d fuzz/corpus ]; then pass "Seed corpus (fuzz/corpus)"; else miss "Seed corpus (fuzz/corpus)"; fi
if ls fuzz/*.{dict,txt} >/dev/null 2>&1; then pass "Fuzz dictionary"; else miss "Fuzz dictionary (fuzz/*.dict)"; fi
if [ -d afl ] || (has_yml && grep -Rqi 'cargo-afl\|afl-fuzz' .github/workflows/*.yml 2>/dev/null); then
  pass "AFL harness/CI present"
else
  miss "AFL harness/CI"
fi
if has_yml && grep -Rqi 'asan\|ubsan\|tsan\|-Z *sanitizer\|RUSTFLAGS:.*sanitizer' .github/workflows/*.yml 2>/dev/null; then
  pass "Sanitizer builds wired in CI"
else
  miss "Sanitizer builds in CI (ASAN/UBSAN/TSAN)"
fi
if has_yml && grep -Rqi 'fuzz' .github/workflows/*.yml 2>/dev/null; then
  pass "Fuzzing CI job(s)"
else
  miss "Fuzzing CI job(s)"
fi
if has_yml && grep -Rqi 'upload-artifact.*crash\|crashes\|fuzz' .github/workflows/*.yml 2>/dev/null; then
  pass "Upload fuzz crashes as artifacts"
else
  miss "Upload fuzz crashes as artifacts"
fi

# 3) Coverage from fuzzing + threshold gate
if has_yml && grep -Rqi 'grcov\|llvm-cov\|tarpaulin' .github/workflows/*.yml 2>/dev/null; then
  pass "Coverage tool in CI (grcov/llvm-cov/tarpaulin)"
else
  miss "Coverage generation in CI"
fi
if has_yml && grep -Rqi 'coverage.*threshold\|min-coverage\|fail-if-below' .github/workflows/*.yml 2>/dev/null; then
  pass "Coverage threshold gate"
else
  miss "Coverage threshold gate"
fi

# 4) Guard backports for CI branches (needs PR view)
info "Guard backports: requires checking PRs on GitHub (cannot verify locally)."

# 5) Docs/README polish
if [ -f README.md ] && grep -qi 'wire format' README.md; then pass "README: wire format"; else miss "README: wire format"; fi
if [ -f README.md ] && grep -Eqi 'version(ing)?|compat(ibility)?' README.md; then pass "README: versioning/compat policy"; else miss "README: versioning/compat"; fi
if [ -f README.md ] && grep -Eqi 'proto|protoc|prost|regenerat' README.md; then pass "README: proto regen steps"; else miss "README: proto regen steps"; fi
if [ -f README.md ] && grep -Eqi 'renode|fuzz' README.md; then pass "README: how to run Renode+fuzz locally"; else miss "README: Renode+fuzz how-to"; fi
if [ -f README.md ] && grep -Eqi 'ci|workflow|actions' README.md; then pass "README: CI overview"; else miss "README: CI overview"; fi

# 6) Quality gates: clippy, rustfmt, MSRV, cargo-deny
if has_yml && grep -Rqi 'cargo +.*clippy\|cargo clippy' .github/workflows/*.yml 2>/dev/null; then
  if grep -Rqi 'deny(warnings)?\|-- -D warnings' .github/workflows/*.yml 2>/dev/null; then
    pass "clippy fail-on-warnings in CI"
  else
    miss "clippy is in CI but not fail-on-warnings"
  fi
else
  miss "clippy in CI"
fi
if has_yml && grep -Rqi 'cargo fmt.* -- --check\|fmt --check' .github/workflows/*.yml 2>/dev/null; then
  pass "rustfmt check in CI"
else
  miss "rustfmt check in CI"
fi
if [ -f rust-toolchain.toml ]; then
  pass "MSRV pinned via rust-toolchain.toml"
else
  if has_yml && grep -Rqi 'toolchain: *1\.[0-9]+' .github/workflows/*.yml 2>/dev/null; then
    pass "MSRV pinned in CI workflow"
  else
    miss "MSRV pin (rust-toolchain.toml or CI toolchain)"
  fi
fi
if [ -f deny.toml ] || [ -f .deny.toml ] || (has_yml && grep -Rqi 'cargo-deny' .github/workflows/*.yml 2>/dev/null); then
  pass "cargo-deny configured"
else
  miss "cargo-deny (optional)"
fi

# 7) Release hygiene: tags, badges
if git tag --list | grep -Eq '^v?[0-9]+\.[0-9]+'; then pass "Git tags exist"; else miss "Release tags"; fi
if [ -f README.md ] && grep -Eqi 'badge|shields\.io|github/actions/workflow/status' README.md; then
  pass "Badges in README (CI/coverage/fuzz)"
else
  miss "Badges in README"
fi

# 8) Optional: crate publish readiness (local checks)
if grep -q '^\[package\]' Cargo.toml; then
  if grep -Eq 'publish *= *false' Cargo.toml; then
    info "Crate set to publish = false"
  else
    pass "Crate metadata present (publishable locally)"
  fi
else
  miss "Cargo.toml [package] section"
fi
echo "== Done =="
