#!/bin/bash
# corpus_sweep.sh — run a shedskin_examples sweep ONCE per (mode, env, tree)
# and cache the result under sweeps/.
#
# Why this exists: a corpus sweep costs 15-40 minutes, and the same one gets
# re-run across sessions because nothing records that it was already done, or
# what tree it was done against. Results here are keyed on the WORKING TREE
# (HEAD's short hash, plus a digest of the uncommitted diff when the tree is
# dirty), so a repeat request on an unchanged tree returns instantly and a
# result from a different tree is never mistaken for a current one.
#
# Usage:
#   ./corpus_sweep.sh [-f] [-m MODE] [-e "VAR=VAL VAR=VAL"] [-t SECS]
#
#   -m MODE   compile  pyc exit status only                    (~15 min)
#             run      + the binary's exit status              (~25 min)
#             check    + warning count, CPython exit status,
#                        and whether stdout MATCHES CPython    (~40 min)
#             default: compile
#   -e ENV    environment for the pyc runs, e.g. -e "PYC_CSELEM=3"
#   -t SECS   per-step timeout (default 400 compile / 120 run)
#   -f        re-run and overwrite even if a cached result exists
#   -l        list cached sweeps and exit
#
# Output: sweeps/<mode>__<env-slug>__<tree>.tsv, plus a row in sweeps/INDEX.md.
# Results are TEXT and are meant to be committed -- they are a record of what
# has been measured, not a build artifact.
#
# Reading the result: the summary is printed on completion and can be
# re-printed for any cached sweep by re-running the same command.
set -u
ROOT=$(cd "$(dirname "$0")" && pwd)
SWEEPS=$ROOT/sweeps
MODE=compile
ENVS=""
FORCE=0
TMO=""

while getopts "fm:e:t:lh" o; do
  case "$o" in
    f) FORCE=1 ;;
    m) MODE=$OPTARG ;;
    e) ENVS=$OPTARG ;;
    t) TMO=$OPTARG ;;
    l) [ -f "$SWEEPS/INDEX.md" ] && cat "$SWEEPS/INDEX.md" || echo "no sweeps recorded"; exit 0 ;;
    h|*) sed -n '2,30p' "$0"; exit 0 ;;
  esac
done

case "$MODE" in
  compile|run|check) ;;
  *) echo "unknown mode '$MODE' (compile|run|check)" >&2; exit 2 ;;
esac

# --- the tree key -------------------------------------------------------
# HEAD alone is not enough: almost every sweep in practice runs on a dirty
# tree mid-change, and two different dirty trees on the same HEAD must not
# share a cache entry. The diff digest makes an unchanged dirty tree cache
# correctly while any edit invalidates it.
HEAD=$(cd "$ROOT" && git rev-parse --short HEAD 2>/dev/null || echo nogit)
if [ -n "$(cd "$ROOT" && git status --porcelain 2>/dev/null)" ]; then
  DIFF=$(cd "$ROOT" && { git diff HEAD; git status --porcelain; } | sha1sum | cut -c1-8)
  TREE="$HEAD+$DIFF"
else
  TREE="$HEAD"
fi
ENVSLUG=$(printf '%s' "${ENVS:-default}" | tr ' =' '__' | tr -cd 'A-Za-z0-9_+.-')
KEY="${MODE}__${ENVSLUG}__${TREE}"
OUT="$SWEEPS/$KEY.tsv"
LOGS="${TMPDIR:-/tmp}/corpus_sweep.$KEY"

mkdir -p "$SWEEPS"

summarize() {
  local f=$1
  awk -F'\t' '
    /^#/ || /^DONE/ { next }
    { n++
      if ($2 != "0") { cfail++; cf = cf " " $1 }
      else {
        if ($3 != "-" && $3 != "0") warned++
        if ($4 != "-" && $4 != "0") { rfail++; rf = rf " " $1 }
        if ($6 == "NO") { diff++; df = df " " $1 }
      }
    }
    END {
      printf "programs=%d compile_fail=%d run_fail=%d stdout_differs=%d with_warnings=%d\n",
             n, cfail, rfail, diff, warned
      if (cfail) printf "  compile-fail:%s\n", cf
      if (rfail) printf "  run-fail:%s\n", rf
      if (diff)  printf "  stdout-differs:%s\n", df
    }' "$f"
}

if [ -f "$OUT" ] && grep -q '^DONE' "$OUT" && [ "$FORCE" = 0 ]; then
  echo "cached: $OUT"
  sed -n '2,4p' "$OUT" | sed 's/^/  /'
  summarize "$OUT"
  exit 0
fi

CT=${TMO:-400}
RT=${TMO:-120}
mkdir -p "$LOGS"
{
  echo "# key   $KEY"
  echo "# mode  $MODE   env: ${ENVS:-(default)}"
  echo "# tree  $TREE"
  echo "# date  $(date -Is)"
  printf 'name\tcompile_rc\twarns\trun_rc\tcpy_rc\tstdout_match\n'
} > "$OUT"

for d in "$ROOT"/shedskin_examples/*/; do
  name=$(basename "$d")
  [ -f "$d$name.py" ] || continue
  cd "$d" || continue
  clog="$LOGS/$name.compile"
  if [ -n "$ENVS" ]; then
    env $ENVS timeout "$CT" "$ROOT/pyc" -D "$ROOT" "$name.py" > "$clog" 2>&1
  else
    timeout "$CT" "$ROOT/pyc" -D "$ROOT" "$name.py" > "$clog" 2>&1
  fi
  crc=$?
  warns=$(grep -c "warning:" "$clog")
  if [ "$crc" != 0 ] || [ "$MODE" = compile ]; then
    printf '%s\t%s\t%s\t-\t-\t-\n' "$name" "$crc" "$warns" >> "$OUT"
    continue
  fi
  timeout "$RT" "./$name" > "$LOGS/$name.pyc.out" 2> "$LOGS/$name.pyc.err"
  rrc=$?
  if [ "$MODE" = run ]; then
    printf '%s\t0\t%s\t%s\t-\t-\n' "$name" "$warns" "$rrc" >> "$OUT"
    continue
  fi
  timeout "$RT" python3 "$name.py" > "$LOGS/$name.cpy.out" 2> "$LOGS/$name.cpy.err"
  prc=$?
  same="-"
  if [ "$rrc" = 0 ] && [ "$prc" = 0 ]; then
    if cmp -s "$LOGS/$name.pyc.out" "$LOGS/$name.cpy.out"; then same=yes; else same=NO; fi
  fi
  printf '%s\t0\t%s\t%s\t%s\t%s\n' "$name" "$warns" "$rrc" "$prc" "$same" >> "$OUT"
done
echo DONE >> "$OUT"

SUM=$(summarize "$OUT")
echo "$SUM"
echo "results: $OUT   per-program logs: $LOGS"

touch "$SWEEPS/INDEX.md"
if ! grep -q "^| \`$KEY\`" "$SWEEPS/INDEX.md" 2>/dev/null; then
  if [ ! -s "$SWEEPS/INDEX.md" ]; then
    {
      echo "# Corpus sweep results"
      echo
      echo "Written by \`corpus_sweep.sh\`. One row per sweep; the file named in"
      echo "each row has the per-program detail. \`tree\` is HEAD's short hash, plus"
      echo "a digest of the uncommitted diff when the sweep ran on a dirty tree."
      echo
      echo "| key | date | result |"
      echo "|---|---|---|"
    } > "$SWEEPS/INDEX.md"
  fi
  printf '| `%s` | %s | %s |\n' "$KEY" "$(date -I)" "$(echo "$SUM" | head -1)" >> "$SWEEPS/INDEX.md"
fi
