# 041 — `struct`, `colorsys`, `getopt`, and `os`'s filesystem functions are no-op stubs that silently produce wrong results

**Status:** open, `struct`/`hashlib` only — `colorsys` fixed
2026-08-08, `getopt` and `os`'s filesystem functions (plus
`os.path.isdir`/`exists`/`islink`) fixed 2026-08-11, `sys.version` and
`string.capwords`/`split`/`join` (found via the same broader audit,
were also silent-wrong stubs) fixed alongside them — see "getopt/os/
sys/string fixed" below. `struct`/`hashlib` assessed for real
implementations the same day and deferred — not hard in themselves,
but blocked on two actual compiler/runtime feature gaps (see their own
files' comments and the summary below). Found 2026-08-08 while
diagnosing [issues/025](025-shedskin-examples-coverage.md)'s TODO list
item 14 ("stdlib long tail: `struct`, `colorsys`, `array`, `re`,
`getopt`, `os.path`, `fnmatch` — no shims"). That claim was wrong at
the file level — all seven already have a `pyc_lib/*.py` shim, most
added by commit `ed00e7c5` (2026-07-08) — but auditing each module's
actual behavior (not just its presence) found a real, more specific
problem underneath the stale claim.

**Affects:** `pyc_lib/struct.py`, `pyc_lib/colorsys.py`,
`pyc_lib/getopt.py`, and the filesystem-touching functions in
`pyc_lib/os.py` (`listdir`, `walk`, `system`, `chdir`, `rename`,
`remove`, `mkdir`, `getcwd`, `stat`, plus `os.path.isdir`/`exists`/
`islink`).

## Per-module status, verified 2026-08-08 (not all seven are the same)

- **`struct`** — **no-op stub.** `pack()` always returns `b""`,
  `unpack()`/`unpack_from()` always return `()`, `calcsize()` always
  returns `0`, regardless of the format string or arguments. The stub
  comment in the file already says this is deliberate, deferred
  scope. `struct.pack('>I', 42)` → CPython `b'\x00\x00\x00*'`; pyc
  `b''`. Real corpus examples depend on correct output: `minpng.py`
  builds an actual PNG file byte-by-byte via `struct.pack`;
  `sha.py` computes a SHA hash the same way. Both would currently
  compile, run, and produce a **plausible-looking but completely
  wrong** PNG file / hash, not a crash or diagnostic.
- **`colorsys`** — **no-op stub.** Every function (`rgb_to_hsv`,
  `hsv_to_rgb`, `rgb_to_hls`, `hls_to_rgb`, `rgb_to_yiq`, `yiq_to_rgb`)
  unconditionally returns `(0.0, 0.0, 0.0)`. `colorsys.hsv_to_rgb(0.5,
  1.0, 1.0)` → CPython `(0.0, 1.0, 1.0)`; pyc `(0.0, 0.0, 0.0)`.
  `mandelbrot2.py` uses `hsv_to_rgb` to build its entire color
  palette — with the stub, every generated color is black; the
  program would run to completion and emit a valid-looking but
  monochrome image.
- **`getopt`** — **no-op stub.** `getopt()`/`gnu_getopt()`
  unconditionally return `([], [])` regardless of `args`. `getopt.
  getopt(['-f', 'foo.txt', 'bar'], 'f:')` → CPython `([('-f',
  'foo.txt')], ['bar'])`; pyc `([], [])`. `msp_ss.py` and
  `voronoi2.py` parse real CLI options this way — any invocation with
  actual flags would silently ignore all of them and fall through to
  defaults, with no error telling the user why their flags did
  nothing.
- **`os`** — **mixed.** `os.path`'s pure string functions (`join`,
  `split`, `dirname`, `basename`, `splitext`) are genuinely correct —
  verified byte-identical to CPython. Everything that touches the
  real filesystem is a stub: `listdir`/`walk` always return `[]`,
  `system`/`chdir`/`rename`/`remove`/`mkdir` always return `0` (fake
  success), `getcwd` always returns `""`, `stat` always returns an
  all-zero tuple, and `os.path.isdir`/`exists` are hardcoded `True`
  / `islink` hardcoded `False` regardless of the actual path.
  `rdb.py` calls `os.listdir` to enumerate a directory — with the
  stub it always iterates zero files, silently doing nothing instead
  of erroring.
- **`array`** — genuinely functional for construction/indexing/
  iteration (a real list wrapper), but missing methods some corpus
  examples need (`.tofile()` — see
  [issues/025](025-shedskin-examples-coverage.md)'s `mao.py` finding,
  item 12) — an incompleteness gap, not a silent-wrong-output one.
- **`re`** — genuinely functional, a real ~640-line backtracking
  regex engine. Deliberately narrow surface (`match`/`fullmatch`/
  `compile` only — `search`/`sub`/`findall`/`split` are missing,
  attributed in the file's own header comment to two specific,
  cross-referenced compiler bugs, not laziness). An honest
  incompleteness gap, not silently wrong.
- **`fnmatch`** — genuinely, fully functional (a real backtracking
  wildcard matcher). Verified byte-identical to CPython. No gap at
  all; the original TODO claim was simply wrong for this one.

## Why the stub cases matter more than the incomplete ones

`array`/`re`'s gaps fail *loudly* — calling an unimplemented method
either doesn't compile or (for `re`) is a deliberately absent
function, easy to notice. `struct`/`colorsys`/`getopt`/`os`'s
filesystem stubs fail *silently* — the program compiles, runs to
completion, and produces plausible-looking output that is simply
wrong, with no diagnostic anywhere. This is the same severity class as
[issues/040](closed/040-percent-format-float-arg-int-specifier-garbage.md)
(also found via this same TODO-list audit): a user has no way to
discover the bug short of comparing output against a real reference.

## What a fix needs

Each stub is its own bounded, well-specified piece of work (real
struct format-string pack/unpack for the common type codes actually
used in the corpus — `B`/`H`/`I`/`b`/`h`/`i` plus `<`/`>` endianness
covers `minpng`/`sha`; real `colorsys` HSV/HLS/YIQ conversions are
standard, unambiguous formulas; real `getopt` needs actual
short/long-option parsing matching CPython's error behavior; `os`'s
filesystem functions need to call the real underlying syscalls via
`__pyc_c_call__`, mirroring how `pyc_lib/time.py`/`random.py` already
wrap libc). None attempted here — flagging the precise, verified scope
of each rather than a vague "long tail" is this issue's contribution;
picking one up is a separate task.

## Verification plan

- Per module: a minimal repro comparing pyc's output to CPython's for
  a handful of representative calls (already established above for
  all four) — extend to whichever module is picked up, matching
  CPython exactly, not just "doesn't crash."
- Re-test `minpng.py`/`sha.py` (struct), `mandelbrot2.py` (colorsys),
  `msp_ss.py`/`voronoi2.py` with real CLI flags (getopt), `rdb.py`
  (os.listdir) end-to-end once a fix lands for that module.
- Full `test_pyc.py` both backends.

## What this unblocks

Correct behavior for any program depending on real `struct` packing,
`colorsys` conversions, `getopt` CLI parsing, or `os` filesystem
queries — confirmed to affect `minpng`, `sha`, `mandelbrot2`, and
(partially) `rdb`/`msp_ss`/`voronoi2` in the shedskin corpus, silently
today rather than with a diagnostic.

## `colorsys` fixed (2026-08-08)

Ported `pyc_lib/colorsys.py` from CPython's own `Lib/colorsys.py`
(`rgb_to_yiq`/`yiq_to_rgb`/`rgb_to_hls`/`hls_to_rgb`/`rgb_to_hsv`/
`hsv_to_rgb`) — real HSV/HLS/YIQ conversions instead of the
`(0.0, 0.0, 0.0)` stub. `struct`/`getopt`/`os` filesystem functions
remain untouched stubs; this issue stays open for those.

**Two genuine, general compiler bugs found and fixed along the way**
(colorsys's `_v()` helper needs `hue % 1.0`, exposing both):

1. **`%` had no float support at all, three separate layers deep.**
   `pyc_c_runtime.h`'s `_CG_prim_mod` used raw C `%` (invalid on
   `double` — a hard C compile error); `cg_emit_llvm.cc`'s
   `P_prim_mod` used LLVM's `srem` unconditionally (wrong for floats,
   needs `frem`); and `ifa/if1/num.cc`'s constant-folder used
   `DO_FOLDI` (int-only by design — `hue % 1.0` with two compile-time
   *literals* hit `assert(!"case")` in the compiler itself, a
   different failure from the runtime path). Fixed all three: an
   overloaded `_CG_mod_impl` (int64/double) in the C runtime header
   (guarded `#ifdef __cplusplus`, matching the existing
   `_CG_prim_primitive_to_string` precedent, since this header is also
   compiled as plain C for the LLVM backend's linked runtime); a
   `frem`-vs-`srem` split via `is_float` in the LLVM emitter; a new
   `DO_FOLDMOD` fold macro (mirrors the existing `DO_FOLD`/`DO_FOLDI`/
   `DO_FOLDB` family's structure) using `fmod` for the float case.
2. **Even plain `int % int` had the wrong sign.** `-7 % 3` gave `-1`
   (C's/`fmod`'s truncated-toward-zero remainder) instead of Python's
   `2` (floored remainder — result takes the *divisor's* sign, not the
   dividend's). Fixed in the same three places with the standard
   truncated-to-floored adjustment (`if (r != 0 && (r<0) != (b<0)) r
   += b`).
   Also updated `%`'s declared primitive argument type
   (`ifa/prim_data.dat` / `ifa/if1/prim_data.cc`) from
   `PRIM_TYPE_ANY_INT_A/B` to `PRIM_TYPE_ANY_NUM_A/B` (matching `*`/
   `/`/`+`/`-`/`**`, which already declare `ANY_NUM`) — without this,
   float `%` still ran correctly in the default tolerant compile mode
   but was a **hard compile-time error under `-r`** (strict mode),
   since FA's own declared-type check for the primitive was still
   int-only even after the codegen/runtime fix.

**Verified:** all 10 sign/type combinations (`-7%3`, `7%-3`, `-7%-3`,
`7%3`, float versions, mixed int/float) match CPython exactly, on both
backends, both as compile-time constants and through runtime
variables; `ifa`'s own `make test` (all phases, `ifa-test` UnitTest
included) clean; full `test_pyc.py` both backends clean (263/12/0/4).
New tests `tests/modulo_float_and_sign.py`,
`tests/colorsys_module.py` (the latter needs a
`.python.expect_fail` sidecar — pyc's `str(float)` is more verbose
than CPython's `repr` shortest-round-trip form, e.g.
`0.90000000000000002` vs `0.9`; a separate, pre-existing, already-
documented formatting gap, not a value mismatch).
`shedskin_examples/mandelbrot2/mandelbrot2.py` now compiles clean and
produces a genuinely multi-colored BMP (spot-checked pixel bytes —
was all-black before, per this issue's own original finding).

**A third, separate bug found, filed, and since fixed** (2026-08-10,
see [ifa/issues/092](../ifa/issues/closed/092-DISPATCH-3arg-minmax-plus-multi-shape-return-crash.md)
for the full writeup): CPython's own `rgb_to_hls`/`rgb_to_hsv` use
`max(r, g, b)`/`min(r, g, b)` (3-arg builtin calls), which crashed the
caller at runtime (`"matching function not found"`) — turned out to be
a plain arity bug, not the FA/dispatch mystery it first looked like:
`__pyc__/05_builtins.py`'s `min`/`max` only accepted two positional
values, so the 3rd silently misbound into the `key=` formal. Fixed by
giving `min`/`max` an explicit `c=None` third positional slot;
`pyc_lib/colorsys.py` now calls the real builtin `max(r, g, b)`/
`min(r, g, b)` directly (the `_max3`/`_min3` workaround helpers were
removed).

## `getopt`/`os`/`sys.version`/`string` fixed (2026-08-11)

**`getopt`/`gnu_getopt`**: real short/long-option parsing ported from
CPython's own `Lib/getopt.py` algorithm (prefix-matching long options,
`--` end-of-options, `GetoptError` with the same conditions CPython
raises it), pure Python, no runtime changes. Verified byte-for-byte
against CPython across short options, long options with `=` and with
a separate argument, `gnu_getopt`'s intermixed-args behavior, and the
`GetoptError` case, on both backends.

**`os` filesystem functions + `os.path.isdir`/`exists`/`islink`**:
most of libc's filesystem calls are directly reachable via
`__pyc_c_call__` as-is — a pyc `_CG_string` is already a valid
NUL-terminated `const char*`, matching the existing `pyc_lib/time.py`
pattern (`__pyc_c_call__(int, "time", int, 0)`), so `chdir`/`rename`/
`unlink`/`mkdir`/`system`/`access` needed no new runtime code at all.
Added a handful of small C helpers to `pyc_c_runtime.h`/`pyc_runtime.c`
(same `extern`-forces-out-of-line-emission pattern as the existing
`_CG_fopen`/`_CG_argv_at` family) for the few that return a struct or
a pointer that needs smuggling through `int64`: `_CG_getcwd`,
`_CG_is_dir`/`_CG_is_symlink` (`stat`/`lstat` + `S_ISDIR`/`S_ISLNK`),
`_CG_opendir`/`_CG_readdir_name`/`_CG_closedir`, and
`_CG_stat_int_field` (one `stat()` call per requested field, matching
CPython's own documented 10-tuple backward-compat view of
`os.stat_result` — which is all-integer, truncated-seconds times, not
mixed int/float — so the result stays a uniform int64 tuple rather
than tripping the heterogeneous-tuple gap below). `os.walk` is a
plain function (not a generator) driven by an explicit stack rather
than recursion, to avoid depending on recursive-generator support.
Verified against CPython on both backends with a real directory tree
exercising every function (`listdir`, `path.isdir`/`exists`/`islink`,
`getcwd`, `mkdir`, `rename`, `stat`, `remove`, `walk`) — all outputs
matched exactly.

**Bonus, found during the same audit** (not originally in this issue's
scope, but the same silent-wrong-value class): `sys.version` was
hardcoded `"2.7.18"` — a Python-2-migration leftover, actively
misleading for a Python-3-targeting compiler, and
`shedskin_examples/circle/circle.py` does `print(sys.version)`
directly. Now `"3.11.0 (pyc)"` (not a specific real CPython build
string, since this isn't CPython). Also added `sys.version_info` and
`sys.platform` (`circle_main.py` checks `sys.platform == 'win32'`,
previously an undefined name). `string.capwords`/`split`/`join` were
also no-op stubs (always `""`/`[]`); implemented for real (`split`/
`join` don't exist in real Python 3's `string` module at all —
verified against CPython 3.12 — kept anyway in the same Python-2-compat
spirit as the `letters`/`lowercase`/`uppercase` aliases already in that
file).

**A new LLVM-only bug found while verifying `getopt`**, filed
separately: [ifa/issues/095](../ifa/issues/095-LLVM-str-or-none-union-wrong-value.md)
— a `str | None` local's `is not None` check reads back wrong
specifically on `-b`; the C backend is correct. `getopt.py` itself has
a comment flagging this; not fixed here.

**Full verification**: `test_pyc.py` clean on both backends (265
passed / 14 expected-fail / 0 failed / 4 skipped, same baseline as
before this session's changes). Compile-only `shedskin_sweep.sh`:
**two newly-failing examples, `msp_ss` and `rdb` — not regressions**.
Both previously "compiled" only because their `getopt`-gated
(`msp_ss.py`) and `os.listdir`-gated (`rdb.py`) code paths were
providably unreachable under the old always-empty stubs (`getopt`
always returning `([], [])`, `os.listdir` always returning `[]`) and
so got dead-code-eliminated before ever being type-checked. With real
implementations those paths are live, and each hits its own
pre-existing, unrelated bug — both traced to **already-known,
already-tracked mechanisms**, confirmed 2026-08-11: `msp_ss.py`
cascades into several "cannot convert `_CG_any`" C compile errors at
`_CG_fopen`/`_CG_chr`/`_CG_str_to_int64_base` — exactly
[closed/077](../ifa/issues/closed/077-primitive-equality-codegen-missing-salvage-guard.md)'s
own documented, deliberately-unfixed remainder (that issue's fix is a
narrow whitelist of `str`-comparison call sites; its own text already
says every other `__pyc_c_call__` site "is completely unchecked and
unaffected"). `rdb.py` hits `sizeof_element of non-container type
'str' (in __add__) — FA specialized a container method against a
scalar` — [issues/018](018-dict-mixed-key-types-boxing-failure.md)'s
own mechanism (a shared container method not cloned per element/
receiver type), one step more severe than that issue's core repro (a
genuine non-container reaching the method, not just a differently-typed
scalar). Both cross-referenced in their respective issues rather than
filed fresh — real, separate, pre-existing bugs, newly *visible*
rather than newly *caused*. Confirmed via exact before/after fail-set
diff: every other corpus example's outcome is
byte-identical.

**Not attempted, and why** — `struct` and `hashlib` were assessed for
real implementations the same session and deferred; see the dated
comments at the top of `pyc_lib/struct.py` and `pyc_lib/hashlib.py`
for the full reasoning. Short version: neither's core logic (struct's
format-string bit-packing; a real MD5) is the hard part. Both are
blocked on the same underlying gap — constructing a `bytes` value from
a computed sequence of integers doesn't work in pyc today
(`bytes(a_bytearray)`/`bytes([65, 66])` resolve to "expression has no
type", confirmed with a minimal repro) — and `struct.pack`'s real
signature additionally needs `*args` in a function definition, which
`ROADMAP.md` already tracks as parsed-but-not-compiled (confirmed with
a minimal repro: any call through a `*args`-taking function currently
fails with "matching function not found" at runtime). Revisit both
once either lands.
