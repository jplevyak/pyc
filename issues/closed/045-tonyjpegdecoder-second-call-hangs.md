# 045 — tonyjpegdecoder: ~~a second call to `main()` hangs~~ — root-caused 2026-08-15: it is neither a hang nor the second call

> **CLOSED 2026-08-15** — fixed by the `list.__pyc_tobytes__` half of
> [050](../050-pyc-string-builders-are-quadratic.md). tonyjpegdecoder now
> completes all 20 iterations and its decoded BMP is byte-identical to
> CPython's (same md5).
>
> **The title is wrong and is kept only so existing links resolve.**
> Bisected: nothing hangs and nothing is stateful. `bytes(a_list)` is
> **O(n²)** in pyc's own builtin library, so each `main()` takes ~15 s on
> this ~250 KB image and a 20-iteration run simply exceeds any timeout.
> The general defect is filed as
> [050](../050-pyc-string-builders-are-quadratic.md); this issue is its first
> victim and should be closed with it.

## Bisection, 2026-08-15

Every earlier claim here about "the second call" was mistaken:

- 1, 2, 3, 5 and **6** iterations all complete correctly. Only the
  **7th** fails to finish, and only against the clock — no state is
  carried between calls.
- The "first call works, second hangs" reading came from buffered
  stdout: on `timeout`'s kill the buffer is lost, so a run that had
  converted six images looked like it had printed nothing. With
  `stdbuf -oL` the progress is visible.
- **Source instrumentation does not work on this bug.** Adding `print`
  markers inside `InitDecoder` made the symptom disappear — not a
  Heisenbug in the runtime, but pyc compiling a *different program*. The
  bisection had to leave the binary byte-identical.
- `ptrace_scope=1` forbids attaching to a running process, so the sample
  was taken by running the binary as gdb's own child and interrupting it
  from outside. That technique is the reusable part.

The stack, sampled mid-"hang":

```
#0  GC_mark_from ...
#5  GC_alloc_large ...
#8  _CG_string_alloc (s=46304)
#9  _CG_strcat (a=<46 KB buffer>, b="B")
#10 _CG_f_2396_170  /* list::__pyc_tobytes__ */
#11 _CG_f_13457_126 /* main */
```

Appending one byte to a 46 KB string, inside pyc's `list.__pyc_tobytes__`
(`r = r + chr(v)` per element), with the time going to the collector
walking the discarded buffers. `main()` calls `bytes(bmpout)` on the
decoded image every iteration.

So: not the decoder, not `InitDecoder`, not the Huffman loop, not shared
prototype state, and not the second call.


**Status:** open, found 2026-08-08 while diagnosing
[issues/025](../025-shedskin-examples-coverage.md)'s TODO list item 5.
The doc's own "tonyjpegdecoder crashes the compiler with an FPE"
claim is **stale** — re-verified today, the compiler no longer
crashes at all (see the two fixes below); this issue tracks the
*current*, real blocker found once those were out of the way.

**Affects:** `shedskin_examples/tonyjpegdecoder/tonyjpegdecoder.py`,
default (non-`-r`) C backend. Not yet narrowed to a specific function
or file — see "Why not root-caused further" below.

## Two real bugs found and fixed getting here

1. **`bytes(x)` never checked for a user-defined `__bytes__`.**
   `python_ifa_build_if1.cc`'s `bytes(x)` intercept dispatches to
   `__pyc_tobytes__` — pyc's own internal name, defined only by
   `bytes`/`str`/`list` themselves — never CPython's real `__bytes__`
   dunder. `BMPFile` (this file) defines `__bytes__`, matching real
   Python, and had no way to be reached: `bytes(a_bmpfile_instance)`
   failed to type. Fixed with a default `object.__pyc_tobytes__` that
   calls `self.__bytes__()` (`__pyc__/00_runtime.py`) — a class with
   neither gets the usual "unresolved call" reject, matching every
   other unimplemented-dunder case in this codebase. New regression
   test `tests/bytes_user_dunder.py`.
2. **LLVM backend: `_CG_string_identity` was never linkable.** Once
   (1) was fixed, `bytes(already_a_bytes_value)` (the identity case,
   `bytes`'s own `__pyc_tobytes__`) still failed on `-b` — a plain
   `undefined reference to _CG_string_identity` at link time.
   `pyc_c_runtime.h` defines it `inline`; `pyc_runtime.c` (which
   force-exports every inline runtime helper the LLVM backend calls
   by name — see that file's own header comment) was simply missing
   this one `extern` declaration. Added it. Confirmed pre-existing
   and unrelated to (1) via a bare `bytes(b"hello")` repro.

## Current blocker

With both of the above fixed, `pyc -D ... tonyjpegdecoder.py`
(**without** `-r` — see the `-r`-specific oddity below) compiles
clean, and the resulting binary **runs correctly for one full
decode-and-write cycle**, matching CPython's own stdout trace nearly
exactly (`marker d8` / `marker e0` / ... / `jpeg header read, 257 x
323` / `converted <instance> to tiger1.bmp` — the `<instance>` vs.
CPython's `<_io.BufferedReader name='tiger1.jpg'>` is a separate,
cosmetic `%s`-of-a-file-object mismatch, not investigated).

The program's own `__main__` block calls `main()` in a loop (20
times, for timing). **The second call hangs**: `ps` shows sustained
100% CPU with zero output progress for 20+ seconds (vs. CPython's
~150ms/call). Minimized to a clean, reproducible 2-call repro
(`tonyjpegdecoder.py`'s body with the `__main__` block replaced by
`for n in range(2): main()`) — iteration 1 completes and matches
CPython exactly; iteration 2 stalls right after printing "jpeg header
read, 257 x 323" (i.e., somewhere inside `InitDecoder()` or the
Huffman/MCU decode loop that follows it — not narrowed further).

Each `main()` call constructs an entirely fresh `TonyJpegDecoder()`,
`inputfile`, etc. — nothing in the Python source is shared across
calls. This shape (independently-constructed instances behaving as
if they share state) matches the general *family* of bugs this
session already found and fixed twice this week —
[issues/closed/017](017-multi-instance-mutation-corruption.md)
(dict/set sharing a class-body prototype's mutable list fields) and
[issues/closed/044](044-list-add-mutates-receiver.md)
(`list.__add__` mutating its receiver) — but neither of those
specific mechanisms was confirmed here: `TonyJpegDecoder.__init__`
already gives every mutable field (lists, dicts, `HUFFTABLE()`
instances) a fresh value the correct way (matching 017's fix
pattern), and no obvious `list + [...]` accumulation pattern
(044's shape) appears in the decode path on a quick read. This is a
**hypothesis to check first**, not a confirmed cause.

## The `-r` flag oddity, explained (2026-08-08, while implementing ifa/085's fix)

`pyc -r tonyjpegdecoder.py` (with `-r` passed) fails outright at
compile time: `fail: unable to resolve to a single function at call
site` — the exact message
[ifa/issues/090](../../ifa/issues/closed/090-CGEN-tuple-arity-cant-vary-across-loop-iterations.md)
documents. This originally looked backwards against `if
(!fruntime_errors) fail(...)` (`ifa/codegen/cg.cc:956`) — passing `-r`
seemed like it should *suppress* this fail, not cause it. Resolved
while implementing [ifa/085](../../ifa/issues/closed/085-CGEN-dead-if-unresolved-condition-no-guard.md)'s
fix: `-r`/`--runtime_errors` is a **negative** flag — `pyc.cc`'s
`ArgumentDescription` entry uses type code `'f'` (lowercase), which
`ifa/common/arg.cc` sets to `false` when the flag is *given*; the
default (unset) value is `true` (`defs.h`). So the *default* (no `-r`)
is the tolerant, salvage-and-continue mode
(`fruntime_errors == true`), and passing `-r` **disables** that,
forcing hard compile-time errors — the opposite of what the flag's
name suggests. Not backwards at all once the polarity is known: `-r`
turning a salvageable violation into a hard fail is exactly what a
"stop tolerating type violations" flag should do.

## Minimisation attempted and rejected (2026-08-15)

Re-verified still real on current HEAD: compiles (rc=0), then hangs —
120 s timeout, zero output.

The obvious reduction **does not reproduce**: a class holding a
`[0] * n` buffer and a position index, mutated in a `while` loop, with
`main()` constructing a fresh instance and being called twice, compiles
and prints the right answer both times. So "call a function twice with
fresh objects" is not by itself the trigger, and landing that program as
this issue's test would have recorded a false negative.

Narrowing has to come from bisecting the real file (deleting decode
stages until the hang goes away), not from guessing a small one.

## Why not root-caused further here

Full root-causing would mean either (a) tracing the generated C for a
real ~250-line JPEG Huffman decoder's second-call state (this
session's established technique for prior bugs, but a much bigger
surface than any single-function repro traced so far), or (b)
bisecting `TonyJpegDecoder`'s ~20 fields and dozen-odd methods down to
a minimal synthetic repro — neither attempted, to keep this session's
per-item time bounded, matching the "file, don't force" precedent for
similarly deep items (see issues/025's items 2's original
investigation before its eventual fix, or ifa/issues/086,087,088,090
this session).

## Verification plan once fixed

- The 2-call repro above (`tonyjpegdecoder.py` body + `for n in
  range(2): main()`) must complete in comparable time to CPython
  (order of hundreds of ms, not 20+ seconds) and produce matching
  `tiger1.bmp` output for both calls.
- The real `shedskin_examples/tonyjpegdecoder/tonyjpegdecoder.py`
  (unmodified, all 20 iterations) should then be re-timed against
  CPython's `TIME 2.95`-style summary line.
- Full `test_pyc.py`, both backends.

## What this unblocks

tonyjpegdecoder joins the corpus's compile-and-run-correctly set for
its first call; the full 20-iteration benchmark (its actual intended
workload) remains blocked on this hang. More generally, if the
eventual root cause turns out to be another shared-prototype/aliasing
bug in the 017/044 family, fixing it could plausibly affect any
program constructing repeated instances of a class with many mutable
fields in a loop — worth checking broadly once found, not just against
this one file.
