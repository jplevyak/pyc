# 041 — `struct`, `colorsys`, `getopt`, and `os`'s filesystem functions are no-op stubs that silently produce wrong results

**Status:** open, found 2026-08-08 while diagnosing
[issues/025](025-shedskin-examples-coverage.md)'s TODO list item 14
("stdlib long tail: `struct`, `colorsys`, `array`, `re`, `getopt`,
`os.path`, `fnmatch` — no shims"). That claim was wrong at the file
level — all seven already have a `pyc_lib/*.py` shim, most added by
commit `ed00e7c5` (2026-07-08) — but auditing each module's actual
behavior (not just its presence) found a real, more specific problem
underneath the stale claim.

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
[issues/040](040-percent-format-float-arg-int-specifier-garbage.md)
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
