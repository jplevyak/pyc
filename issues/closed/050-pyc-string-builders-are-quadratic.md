# 050 — `str.join`, `lower`, `upper`, `replace` are O(n²)

**Status:** closed 2026-09-30. The probe below now runs in 0.05 s (C) and
0.01 s (LLVM); it took 122 s. Filed earlier; last rewritten 2026-09-28
(history: `git show e3b44e2c:issues/050-pyc-string-builders-are-quadratic.md`).
`list.__pyc_tobytes__` was fixed 2026-08-15 (tonyjpegdecoder's hang);
`str.__mul__` and `__pyc_getslice__` were always linear.

Probe: `"".join(["x"] * 400000)` plus `("y" * 400000).upper()`.

## Cause

`__pyc__/01_str.py` builds strings with `r = r + x` in a loop (`join`,
`lower`, `upper`, `replace`, `__pyc_substr__`). Each step copies the
whole prefix: Θ(n²) bytes, plus n dead buffers for the GC. `join` is
the idiom users reach for to AVOID quadratic concatenation, and here it
is just as slow.

## Fix

Pre-compute the length, allocate once, fill: a runtime helper through
`__pyc_c_call__`, the way `list.__mul__` uses `_CG_list_mult`
(`_CG_string_alloc(n)` exists). `join`'s length is the sum of the parts'
lengths plus separators; the case maps are length-preserving.

**One caution, measured on the `tobytes` fix:** replacing a `__pyc__`
loop with a C call changes FA's trajectory. The C-helper version of
`tobytes` made `list.__add__` specialise against a `bytes` receiver, and
`rdb` stopped compiling. If a helper perturbs a program, root-cause the
union it exposes (AGENTS.md: a union pyc invented is pyc's bug). Do not
fall back to a slower library loop to hide it. The chunked-merge
fallback (O(n log n), used for `tobytes`) is acceptable only with that
root cause named.

## Verification

The probe above runs in well under a second. A `tests/` fixture checks
`join`/`upper`/`lower`/`replace` outputs against CPython. A same-binary
corpus `check` shows no verdict change.

## Resolution (2026-09-30)

- **`str.join`** now does what `bytes.join` already did: it collects the
  parts once (`seq` may be a generator), then calls `_CG_string_join`,
  which sums the lengths and fills one allocation. `dead.cc` already keeps
  a list alive when an opaque C call reads it (the minpng fix), so nothing
  new was needed there.
- **`lower`/`upper`/`swapcase`** use `_CG_str_casemap` (ASCII, as before).
  **`replace`** uses `_CG_str_replace`: one counting pass, one allocation.
  **`__pyc_substr__`** uses `_CG_str_substr`. All are in `pyc_c_runtime.h`
  and declared in `pyc_runtime.c` for the LLVM link.
- **A silent wrong answer, fixed on the way:** `s.replace("", x)` returned
  `s` unchanged. CPython inserts `x` around every character
  (`"abc".replace("", "-") == "-a-b-c-"`).
- **`pyc_lib`:** `os._str_sub`, `re` `Match.group`, and `io`'s
  `BytesIO`/`StringIO` `read`/`readline` built substrings one character at a
  time, from before `str` had a slice path. They are plain slices now.
- **Out of scope:** a user's own `s = s + x` loop is still O(n²). CPython
  avoids that with its refcount-1 in-place resize. `bytes.__repr__`
  (`01b_bytes.py`) still builds with `+=`, which is O(n²) in the length of
  a printed bytes object.

**The caution above did not fire.** Same-program corpus `check` against
`check__default__16a380f5+396039f2`: no compile verdict changed, and total
CreationSets are 208 623 vs 208 092 summed over every DEMAND line. The one
run change, `tonyjpegdecoder` 0 → 124, is its wall clock straddling the
120 s cap. It runs in 117.9 s alone, it uses none of the changed methods,
and its analysis is identical (`ess=370 css=1750`). `sieve` shows as
"stdout differs" in this sweep (`check__default__a072e7ab+79351ffb`)
only because its `time: %.2f` lines differ. No other verdict moved.

**Verification.** `tests/str_builders.py` checks `join` (list, generator,
empty, single), the three case maps, `replace` (including empty `old` and
an embedded NUL), `split`, slices, `StringIO`, `re` groups, `os.path`, and a
200 000-character build against CPython, on both backends. `make test`: 375
passed, 0 failed, both backends.
