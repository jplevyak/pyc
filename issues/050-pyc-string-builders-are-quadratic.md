# 050 — `str.join`, `lower`, `upper`, `replace` are O(n²)

**Status:** open. Re-verified 2026-09-28: `"".join(["x"] * 400000)` plus
`("y" * 400000).upper()` takes **122 s** (CPython: instant). Rewritten;
history in git: `git show e3b44e2c:issues/050-pyc-string-builders-are-quadratic.md`.
`list.__pyc_tobytes__` was fixed 2026-08-15 (tonyjpegdecoder's hang);
`str.__mul__` and `__pyc_getslice__` were always linear.

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
