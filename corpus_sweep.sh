#!/bin/bash
# corpus_sweep.sh — run a shedskin_examples sweep ONCE per (mode, env, tree)
# and cache the result under sweeps/.
#
# Why this exists: a corpus sweep gets re-run across sessions because
# nothing records that it was already done, or what tree it was done
# against. Results here are keyed on the WORKING TREE
# (HEAD's short hash, plus a digest of the uncommitted diff when the tree is
# dirty), so a repeat request on an unchanged tree returns instantly and a
# result from a different tree is never mistaken for a current one.
#
# Usage:
#   ./corpus_sweep.sh [-f] [-m MODE] [-e "VAR=VAL"] [-t SECS] [-j N] [-J M] [-R] [-C]
#
#   -m MODE   compile  pyc exit status only                     (~5 min)
#             run      + the binary's exit status              (~11 min)
#             check    + warning count, CPython exit status,
#                        and whether stdout MATCHES CPython    (~12 min warm)
#             default: compile
#   -e ENV    environment for the pyc runs, e.g. -e "PYC_CSELEM=3"
#   -t SECS   per-step timeout (default 400 compile / 120 run)
#   -j N      COMPILE parallelism (default: nproc)
#   -J M      RUN/CPython parallelism (default: max(1, nproc/4))
#   -R        also confirm the pyc RUN phase's timeouts by re-running them
#             alone (CPython's are confirmed unconditionally; see below)
#   -C        ignore the CPython result cache and re-run CPython
#   -f        re-run and overwrite even if a cached sweep result exists
#   -l        list cached sweeps and exit
#
# Output: sweeps/<mode>__<env-slug>__<tree>.tsv, plus a row in sweeps/INDEX.md.
# Results are TEXT and are meant to be committed -- they are a record of what
# has been measured, not a build artifact.
#
# Reading the result: the summary is printed on completion and can be
# re-printed for any cached sweep by re-running the same command.
#
# --- parallelism, and why the two knobs differ --------------------------
# Compiling is a pure CPU job measured only by its exit status, so it runs
# `-j nproc` wide. RUNNING is different: the recorded value is an exit
# status that includes `124` (timed out), and a program near the limit
# flips under load -- `score4`'s runtime straddles the 120 s cap, and one
# such reading survived into an A/B comparison in this repo before being
# caught. So the run and CPython phases default to `nproc/4`.
#
# A parallel pass can only turn a COMPLETION into a TIMEOUT, never the
# reverse, so `rc=124` is the one verdict worth re-taking -- and those are
# re-run ALONE, which is the confirmation the INDEX.md postmortems had to
# do by hand. CPython's timeouts are confirmed unconditionally (a
# fabricated one silently drops the program out of the stdout comparison,
# and the answer is cached, so it is paid once per corpus); the pyc side
# is `-R`, because each confirmation costs a full timeout and it measured
# 0 fabrications in 72 programs at -J8.
#
# Measured against the serial script on this corpus: 76 of 77 programs
# byte-identical. The 77th is `score4`, which straddles the 120 s cap and
# flips across repeats of one build (see sweeps/INDEX.md).
#
# --- the CPython cache --------------------------------------------------
# CPython's exit status and stdout depend on the corpus, not on pyc, and
# 19 of the 77 programs time out under CPython -- 38 minutes of a `check`
# sweep spent re-deriving a constant. They are cached under
# sweeps/cpython-cache/ (gitignored), keyed on the corpus tree hash, any
# uncommitted `shedskin_examples/**/*.py`, and the python3 version, so a
# corpus change invalidates every entry. `-C` forces a re-run.
#   CAVEAT: a few programs read a file their own run rewrites (`oliva2`
#   reads and writes `oliva.pgm`). The cached CPython output was produced
#   after THAT sweep's pyc binary wrote that file. Use `-C` if a change
#   could alter what a program writes into its own inputs.
set -u
ROOT=$(cd "$(dirname "$0")" && pwd)
SELF="$ROOT/$(basename "$0")"   # workers are re-execs; a relative $0 breaks under xargs

# ---- worker mode (re-entrant; invoked by xargs, never by a human) ------
# Each worker writes single-value files into $LOGS and prints nothing, so
# result assembly is a sorted read at the end rather than a racing append.
if [ "${1:-}" = "--worker" ]; then
  phase=$2 name=$3
  d="$ROOT/shedskin_examples/$name"
  cd "$d" || exit 0
  case "$phase" in
    compile)
      clog="$LOGS/$name.compile"
      t0=$(date +%s)
      if [ -n "$ENVS" ]; then
        env $ENVS timeout "$CT" "$ROOT/pyc" -D "$ROOT" "$name.py" > "$clog" 2>&1
      else
        timeout "$CT" "$ROOT/pyc" -D "$ROOT" "$name.py" > "$clog" 2>&1
      fi
      echo $? > "$LOGS/$name.crc"
      echo $(( $(date +%s) - t0 )) > "$LOGS/$name.cwall"
      grep -c "warning:" "$clog" > "$LOGS/$name.warns"
      ;;
    run)
      t0=$(date +%s)
      timeout "$RT" "./$name" > "$LOGS/$name.pyc.out" 2> "$LOGS/$name.pyc.err"
      echo $? > "$LOGS/$name.rrc"
      echo $(( $(date +%s) - t0 )) > "$LOGS/$name.rwall"
      ;;
    cpy)
      if [ "$NOCPYCACHE" = 0 ] && [ -f "$CPYCACHE/$name.rc" ]; then
        cp "$CPYCACHE/$name.rc" "$LOGS/$name.prc"
        [ -f "$CPYCACHE/$name.out" ] && cp "$CPYCACHE/$name.out" "$LOGS/$name.cpy.out"
        echo 0 > "$LOGS/$name.pwall"
        echo hit > "$LOGS/$name.pcache"
        exit 0
      fi
      t0=$(date +%s)
      timeout "$RT" python3 "$name.py" > "$LOGS/$name.cpy.out" 2> "$LOGS/$name.cpy.err"
      prc=$?
      echo $prc > "$LOGS/$name.prc"
      echo $(( $(date +%s) - t0 )) > "$LOGS/$name.pwall"
      echo miss > "$LOGS/$name.pcache"
      mkdir -p "$CPYCACHE"
      echo $prc > "$CPYCACHE/$name.rc"
      cp "$LOGS/$name.cpy.out" "$CPYCACHE/$name.out"
      ;;
  esac
  exit 0
fi

SWEEPS=$ROOT/sweeps
MODE=compile
ENVS=""
FORCE=0
TMO=""
NCPU=$( (nproc 2>/dev/null) || echo 4 )
JC=$NCPU
JR=$(( NCPU / 4 )); [ "$JR" -lt 1 ] && JR=1
RECHECK=0
NOCPYCACHE=0

while getopts "fm:e:t:j:J:RClh" o; do
  case "$o" in
    f) FORCE=1 ;;
    m) MODE=$OPTARG ;;
    e) ENVS=$OPTARG ;;
    t) TMO=$OPTARG ;;
    j) JC=$OPTARG ;;
    J) JR=$OPTARG ;;
    R) RECHECK=1 ;;
    C) NOCPYCACHE=1 ;;
    l) [ -f "$SWEEPS/INDEX.md" ] && cat "$SWEEPS/INDEX.md" || echo "no sweeps recorded"; exit 0 ;;
    h|*) sed -n '2,60p' "$SELF"; exit 0 ;;
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
#
# The digest MUST NOT see what a sweep itself writes, or the cache can
# never hit: finishing a sweep drops a new `sweeps/*.tsv`, adds a row to
# `sweeps/INDEX.md`, and lets every corpus binary rewrite its own output
# files (`chaos/py.ppm`, `tonyjpegdecoder/tiger1.bmp`, ...). All three land
# in `git status` / `git diff HEAD`, so the key computed on the NEXT run
# differed from the one just recorded, every time -- measured, three
# distinct digests from one unchanged source tree. Excluded here:
# `sweeps/`, and everything under `shedskin_examples/` that is not a `.py`
# (a corpus SOURCE change must still invalidate; its OUTPUTS must not).
# Two invocations, not one pathspec: git applies every `:!` exclusion
# AFTER all inclusions, so a re-include of `shedskin_examples/**/*.py`
# alongside `:!shedskin_examples` is silently dropped -- which made a
# corpus SOURCE edit invisible to the key. Verified both ways.
CORPUS_PY=':(glob)shedskin_examples/**/*.py'
HEAD=$(cd "$ROOT" && git rev-parse --short HEAD 2>/dev/null || echo nogit)
KEYDIRT=$(cd "$ROOT" && { git diff HEAD       -- . ':!sweeps' ':!shedskin_examples' 2>/dev/null
                          git status --porcelain -- . ':!sweeps' ':!shedskin_examples' 2>/dev/null
                          git diff HEAD       -- "$CORPUS_PY" 2>/dev/null
                          git status --porcelain -- "$CORPUS_PY" 2>/dev/null; })
if [ -n "$KEYDIRT" ]; then
  DIFF=$(printf '%s' "$KEYDIRT" | sha1sum | cut -c1-8)
  TREE="$HEAD+$DIFF"
else
  TREE="$HEAD"
fi
ENVSLUG=$(printf '%s' "${ENVS:-default}" | tr ' =' '__' | tr -cd 'A-Za-z0-9_+.-')
KEY="${MODE}__${ENVSLUG}__${TREE}"
OUT="$SWEEPS/$KEY.tsv"
LOGS="${TMPDIR:-/tmp}/corpus_sweep.$KEY"

# The CPython cache key deliberately ignores everything pyc-side and
# every corpus file a RUN rewrites: only committed corpus content,
# uncommitted .py edits and the interpreter version can change what
# CPython prints.
CPYKEY=$( { python3 -V 2>&1
            (cd "$ROOT" && git rev-parse "HEAD:shedskin_examples" 2>/dev/null) || echo nogit
            (cd "$ROOT" && git diff HEAD -- 'shedskin_examples/*.py' 2>/dev/null)
            (cd "$ROOT" && git ls-files -o --exclude-standard -- 'shedskin_examples/*.py' 2>/dev/null \
               | xargs -r sha1sum)
          } | sha1sum | cut -c1-12 )
CPYCACHE="$SWEEPS/cpython-cache/$CPYKEY"

mkdir -p "$SWEEPS"

# --- the content key ----------------------------------------------------
# The tree key names a sweep for a HUMAN ("which commit was this?"), and
# it necessarily changes when you COMMIT -- so the measurement you just
# paid ten minutes for is orphaned by the very commit that lands it.
# This second key answers the machine's question instead: is the thing
# under test the same? Everything that can change a sweep's result and
# nothing that cannot --
#   * the `pyc` binary (libifa is linked into it, so this covers ifa too)
#   * `__pyc__/*.py`, which pyc READS at run time rather than linking
#   * every corpus `*.py`
#   * the env overrides and both timeouts (a `-t 20` sweep is not the
#     same measurement as a `-t 120` one -- the old key ignored this)
# It is deliberately conservative: `make clean` re-stamps BUILD_VERSION
# into version.o and changes the binary with no source change, which
# costs a needless re-measure. A false MISS wastes time; a false HIT
# would report a stale answer as current.
CONTENT=$( { sha1sum "$ROOT/pyc" 2>/dev/null || echo nopyc
             cat "$ROOT"/__pyc__/*.py 2>/dev/null | sha1sum
             find "$ROOT/shedskin_examples" -name '*.py' -print0 2>/dev/null \
               | sort -z | xargs -0 -r cat | sha1sum
             printf '%s|%s|%s\n' "${ENVS:-}" "${TMO:-}" "$MODE"
           } | sha1sum | cut -c1-12 )

summarize() {
  local f=$1
  awk -F'\t' '
    /^#/ || /^DONE/ { next }
    /^name\tcompile_rc/ { next }   # the TSV header is not a program
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

if [ "$FORCE" = 0 ]; then
  HIT=""
  if [ -f "$OUT" ] && grep -q '^DONE' "$OUT"; then
    HIT=$OUT
  else
    # Same content measured under a different tree name -- typically the
    # same work, before and after the commit that landed it.
    for f in "$SWEEPS/${MODE}__${ENVSLUG}__"*.tsv; do
      [ -f "$f" ] || continue
      grep -q '^DONE' "$f" || continue
      grep -qx "# content $CONTENT" "$f" || continue
      HIT=$f; break
    done
  fi
  if [ -n "$HIT" ]; then
    [ "$HIT" = "$OUT" ] && echo "cached: $HIT" || echo "cached (same content, measured as $(basename "$HIT" .tsv)): $HIT"
    sed -n '2,5p' "$HIT" | sed 's/^/  /'
    summarize "$HIT"
    exit 0
  fi
fi

CT=${TMO:-400}
RT=${TMO:-120}
rm -rf "$LOGS"; mkdir -p "$LOGS"
export ROOT LOGS ENVS CT RT CPYCACHE NOCPYCACHE

# The program list, in the same alphabetical order the serial script
# produced, so a TSV from this script diffs cleanly against an older one.
PROGS=()
for d in "$ROOT"/shedskin_examples/*/; do
  name=$(basename "$d")
  [ -f "$d$name.py" ] || continue
  PROGS+=("$name")
done

# ---- phase driver ------------------------------------------------------
# `xargs -P` re-invokes this script in --worker mode. A worker never
# writes to the TSV, so there is no append race and no ordering to fix up.
slowest() {  # slowest SUFFIX N -- the phase's long poles, which bound its wall time
  local suf=$1 n=$2 out
  out=$(for f in "$LOGS"/*."$suf"; do
          [ -f "$f" ] || continue
          b=$(basename "$f" ".$suf"); echo "$(cat "$f") $b"
        done | sort -rn | head -"$n" | awk '{printf " %s(%ss)", $2, $1}')
  [ -n "$out" ] && echo "    slowest:$out"
}

run_phase() {  # run_phase PHASE JOBS NAME...
  local phase=$1 jobs=$2; shift 2
  [ $# -eq 0 ] && return 0
  printf '%s\n' "$@" | xargs -r -P "$jobs" -I{} "$SELF" --worker "$phase" {}
}

t_start=$(date +%s)
echo "sweep $KEY: ${#PROGS[@]} programs, mode=$MODE, compile -j$JC, run -J$JR"

# ---- confirmation ------------------------------------------------------
# Contention can only turn a COMPLETION into a TIMEOUT, never the reverse,
# so `rc=124` is the one verdict a parallel pass can fabricate, and the
# only one worth re-taking. Re-run those ALONE. Measured on this corpus at
# -J8: 72 binaries produced zero fabricated run timeouts, and CPython
# produced exactly one -- `hq2x`, which needs 116 s of a 120 s cap and so
# sits on the boundary no -J setting can move it off.
confirm() {  # confirm PHASE RC_SUFFIX LABEL
  local phase=$1 suf=$2 label=$3 n=0 changed=0 after
  for name in "${RUNNABLE[@]}"; do
    [ "$(cat "$LOGS/$name.$suf" 2>/dev/null || echo 0)" = 124 ] || continue
    # A cache HIT was already confirmed by the sweep that produced it.
    [ "$phase" = cpy ] && [ "$(cat "$LOGS/$name.pcache" 2>/dev/null)" = hit ] && continue
    n=$((n+1))
    NOCPYCACHE=1 "$SELF" --worker "$phase" "$name"
    after=$(cat "$LOGS/$name.$suf")
    if [ "$after" != 124 ]; then
      changed=$((changed+1)); echo "    CONFIRM $name $label 124 -> $after (re-run alone)"
    fi
  done
  [ "$n" -gt 0 ] && echo "  confirmed $n $label timeout(s) alone: $changed were contention"
  return 0
}

run_phase compile "$JC" "${PROGS[@]}"
echo "  compiled in $(( $(date +%s) - t_start ))s"; slowest cwall 3

RUNNABLE=()
for name in "${PROGS[@]}"; do
  [ "$(cat "$LOGS/$name.crc" 2>/dev/null || echo 1)" = 0 ] && RUNNABLE+=("$name")
done

if [ "$MODE" != compile ] && [ ${#RUNNABLE[@]} -gt 0 ]; then
  t=$(date +%s); run_phase run "$JR" "${RUNNABLE[@]}"
  echo "  ran ${#RUNNABLE[@]} binaries in $(( $(date +%s) - t ))s"; slowest rwall 3
  # Opt-in: every confirmed timeout costs a full RT and this corpus has
  # ~10 of them, so it doubles a `run` sweep. Measured worth: 0 of 72 at
  # -J8. Turn it on when an A/B shows a single-program run_rc difference.
  [ "$RECHECK" = 1 ] && confirm run rrc run_rc
fi

if [ "$MODE" = check ] && [ ${#RUNNABLE[@]} -gt 0 ]; then
  t=$(date +%s); run_phase cpy "$JR" "${RUNNABLE[@]}"
  hits=$(cat "$LOGS"/*.pcache 2>/dev/null | grep -c hit)
  echo "  CPython: ${#RUNNABLE[@]} programs in $(( $(date +%s) - t ))s ($hits cached, key $CPYKEY)"
  slowest pwall 3
  # NOT opt-in: a fabricated CPython timeout silently drops a program out
  # of the stdout comparison entirely (`stdout_match` becomes `-`), and
  # the confirmed answer is cached, so this is paid once per corpus.
  confirm cpy prc cpy_rc
fi

# ---- assemble ----------------------------------------------------------
{
  echo "# key   $KEY"
  echo "# mode  $MODE   env: ${ENVS:-(default)}"
  echo "# tree  $TREE"
  echo "# content $CONTENT"
  echo "# date  $(date -Is)"
  printf 'name\tcompile_rc\twarns\trun_rc\tcpy_rc\tstdout_match\n'
} > "$OUT"

for name in "${PROGS[@]}"; do
  crc=$(cat "$LOGS/$name.crc" 2>/dev/null || echo 1)
  warns=$(cat "$LOGS/$name.warns" 2>/dev/null || echo 0)
  if [ "$crc" != 0 ] || [ "$MODE" = compile ]; then
    printf '%s\t%s\t%s\t-\t-\t-\n' "$name" "$crc" "$warns" >> "$OUT"
    continue
  fi
  rrc=$(cat "$LOGS/$name.rrc" 2>/dev/null || echo 1)
  if [ "$MODE" = run ]; then
    printf '%s\t0\t%s\t%s\t-\t-\n' "$name" "$warns" "$rrc" >> "$OUT"
    continue
  fi
  prc=$(cat "$LOGS/$name.prc" 2>/dev/null || echo 1)
  same="-"
  if [ "$rrc" = 0 ] && [ "$prc" = 0 ]; then
    if cmp -s "$LOGS/$name.pyc.out" "$LOGS/$name.cpy.out"; then same=yes; else same=NO; fi
  fi
  printf '%s\t0\t%s\t%s\t%s\t%s\n' "$name" "$warns" "$rrc" "$prc" "$same" >> "$OUT"
done
echo DONE >> "$OUT"

SUM=$(summarize "$OUT")
echo "$SUM"
echo "elapsed: $(( $(date +%s) - t_start ))s"
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
  # Insert INTO the table, not at EOF: the file continues with prose and a
  # second (backfill) table below, so `>>` put rows where -l would not show
  # them under the header they belong to.
  ROW=$(printf '| `%s` | %s | %s |' "$KEY" "$(date -I)" "$(echo "$SUM" | head -1)")
  awk -v row="$ROW" '
    !done && /^\|---\|---\|---\|$/ { print; print row; done=1; next }
    { print }' "$SWEEPS/INDEX.md" > "$SWEEPS/INDEX.md.tmp" && mv "$SWEEPS/INDEX.md.tmp" "$SWEEPS/INDEX.md"
fi
