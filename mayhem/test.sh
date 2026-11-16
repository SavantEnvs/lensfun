#!/usr/bin/env bash
#
# mayhem/test.sh — RUN this repo's OWN functional test suite (already built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

# lensfun's suite is a set of glib test binaries (test_*.cpp, plus the deprecated-API ones) that
# read data/db relative to the source root. Run each with --tap and count its "ok"/"not ok" lines:
# a neutered or missing binary prints nothing, so it produces no passes and fails below.
TEST_BUILD="$SRC/mayhem/build-tests"
mapfile -t BINS < <(cd "$TEST_BUILD" && find tests -type f -perm -u+x -name 'test_*' ! -name 'test_modifier_performance' | sort)
[ "${#BINS[@]}" -ge 20 ] || { echo "only ${#BINS[@]} test binaries found (expected >= 20); build.sh bug" >&2; exit 1; }

passed=0; failed=0; skipped=0
for b in "${BINS[@]}"; do
  bin="$TEST_BUILD/$b"
  if [ ! -x "$bin" ]; then echo "MISSING test binary: $bin (build.sh bug)" >&2; failed=$((failed + 1)); continue; fi
  out="$("$bin" --tap 2>&1)"; rc=$?
  p=$(printf '%s\n' "$out" | grep -Ec '^ok [0-9]+ ' || true)
  s=$(printf '%s\n' "$out" | grep -Ec '^ok [0-9]+ .*# SKIP' || true)
  f=$(printf '%s\n' "$out" | grep -Ec '^not ok ' || true)
  echo "$b: ok=$p skip=$s not-ok=$f rc=$rc"
  if [ "$p" -eq 0 ] || [ "$rc" -ne 0 ]; then
    printf '%s\n' "$out" | tail -n 20 >&2
    f=$((f + 1))
  fi
  passed=$((passed + p - s)); skipped=$((skipped + s)); failed=$((failed + f))
done

# Known-answer probe: real database lenses, fixed coordinates, exact distortion results
# (values from tests/test_modifier_regression.cpp).
kat="$("$TEST_BUILD/kat" data/db 2>&1)" || kat="KAT probe failed: $kat"
want='model=smc Pentax-DA 50-200mm f/4-5.6 DA ED
dist 0.000 0.000 -> -14.016 -9.341
dist 751.000 497.000 -> 751.000 497.000
dist 810.000 937.000 -> 810.272 938.967
dist 1270.000 100.000 -> 1275.166 96.035
model=Canon PowerShot G12 & compatibles (Standard)
dist 0.000 0.000 -> 28.806 19.198
dist 751.000 497.000 -> 751.000 497.000
dist 810.000 937.000 -> 809.505 933.423
dist 1270.000 100.000 -> 1260.140 107.568'
if [ "$kat" = "$want" ]; then
  passed=$((passed + 1))
else
  echo "KAT mismatch:" >&2; diff <(printf '%s\n' "$want") <(printf '%s\n' "$kat") >&2 || true
  failed=$((failed + 1))
fi

[ "$passed" -ge 100 ] || { echo "only $passed passing cases; expected >= 100" >&2; failed=$((failed + 1)); }

emit_ctrf "glib-gtest" "$passed" "$failed" "$skipped"
