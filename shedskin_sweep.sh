#!/bin/bash
# shedskin_sweep.sh — measure pyc's coverage of the vendored shedskin
# examples (shedskin_examples/, added via git subtree; see issue 025).
#
# For each example <name>/<name>.py it runs `pyc -D <root> <name>.py`
# in an isolated build dir (a copy of the example, so multi-file
# examples and their data come along) and classifies the outcome by
# the first diagnostic line. Build artifacts stay in the scratch dir,
# never in the repo.
#
# Usage:  ./shedskin_sweep.sh [name-substring]
#   no arg      sweep all examples
#   substring   only examples whose dir name contains the substring
#
# Env:
#   PYC       pyc binary            (default ./pyc)
#   OUTDIR    scratch build root    (default $TMPDIR-ish under /tmp)
#   TIMEOUT   per-example seconds   (default 60)
set -u
ROOT=$(cd "$(dirname "$0")" && pwd)
PYC=${PYC:-$ROOT/pyc}
TIMEOUT=${TIMEOUT:-60}
OUTDIR=${OUTDIR:-/tmp/shedskin_sweep.$$}
FILTER=${1:-}
rm -rf "$OUTDIR"; mkdir -p "$OUTDIR"
RES="$OUTDIR/results.tsv"; : > "$RES"

compiled=0; failed=0
for d in "$ROOT"/shedskin_examples/*/; do
  name=$(basename "$d")
  py="$d$name.py"
  [ -f "$py" ] || continue
  [ -n "$FILTER" ] && [[ "$name" != *"$FILTER"* ]] && continue
  bd="$OUTDIR/$name"; mkdir -p "$bd"
  cp -r "$d"* "$bd/" 2>/dev/null
  out=$(cd "$bd" && timeout "$TIMEOUT" "$PYC" -D "$ROOT" "$name.py" 2>&1)
  rc=$?
  first=$(printf '%s\n' "$out" | grep -m1 -iE "fail|error|unresolved|abort|assert|illegal|has no type|syntax" | head -c 220)
  if [ -z "$first" ] && [ "$rc" -eq 124 ]; then first="(compile timeout ${TIMEOUT}s)"; fi
  if [ -z "$first" ] && [ "$rc" -ge 128 ]; then first="(crash: signal $((rc - 128)))"; fi
  if [ -z "$first" ] && [ -f "$bd/$name.c" ]; then
    printf '%s\tCOMPILED_C\t\n' "$name" >> "$RES"; compiled=$((compiled+1))
  else
    printf '%s\tFAIL\t%s\n' "$name" "$first" >> "$RES"; failed=$((failed+1))
  fi
done

echo "=== pyc -> C: $compiled compiled, $failed failed of $((compiled+failed)) ==="
echo
echo "=== failure buckets (normalized) ==="
cut -f3 "$RES" | sed -E "s/line [0-9]+/line N/; s/'[^']*'/'X'/g; s/[0-9]+/N/g" \
  | sort | uniq -c | sort -rn
echo
echo "full results: $RES"
