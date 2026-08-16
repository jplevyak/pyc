# 050 — every string builder in `__pyc__` is O(n²): `r = r + x` in a loop

**Status:** open, root-caused 2026-08-15 while bisecting
[045](045-tonyjpegdecoder-second-call-hangs.md), which turns out to be
entirely an instance of this.

**Affects:** `__pyc__/01_str.py` (`join`, `lower`, `upper`, `__mul__`,
`replace`), `__pyc__/04_sequence.py` (`list.__pyc_tobytes__`) — every
place that accumulates a string with `r = r + x` inside a loop.

## Measured

```python
xs = [65] * n
b = bytes(xs)          # -> list.__pyc_tobytes__
```

| n | pyc | CPython |
|---|---|---|
| 100 000 | 1 s | 0.000 s |
| 200 000 | 7 s | 0.001 s |
| 400 000 | 25 s | 0.002 s |

Each doubling of `n` roughly quadruples pyc's time — quadratic — while
CPython stays linear. At n = 400 000 pyc is ~12 000× slower.

## Cause

`__pyc__/04_sequence.py`:

```python
def __pyc_tobytes__(self):
    r = ""
    for v in self:
        r = r + chr(v)      # allocates and copies the whole prefix, every element
    return r.encode()
```

Strings are immutable, so each `r + chr(v)` allocates a fresh buffer of
length `len(r)+1` and copies. Over n elements that is Θ(n²) bytes copied
*and* n dead buffers of average length n/2 for the collector to walk — a
gdb sample of the hang lands in `GC_mark_from`, reached from
`_CG_string_alloc` under `_CG_strcat`, not in the decoder at all.

`str.join` has exactly the same shape, which is the usual escape hatch:

```python
def join(self, seq):
    r = ""
    for x in seq:
        if not first: r = r + self
        r = r + x
```

so `"".join(...)` is no faster than the loop it would replace. `lower`,
`upper`, `replace` and `str.__mul__` are all the same pattern.

The existing code comment on `__pyc_tobytes__` shows this was a
deliberate trade — it says the `chr()` loop "avoids a second low-level
buffer-building helper alongside `_CG_string_identity`". The cost of that
choice had simply never been measured.

## Fix direction

A buffer-building runtime helper: `_CG_string_alloc(n)` already exists in
`pyc_c_runtime.h`, and `__pyc__` code can reach C directly via
`__pyc_c_call__` (as `list.__mul__` does with `_CG_list_mult`). For
`list.__pyc_tobytes__` the final length is known up front (`len(self)`),
so it is a single allocate-and-fill. `join` can pre-compute its length
the same way. That fixes the whole family at the root rather than
per-method.

## Verification plan

- The table above becomes linear, and `tests/bytes_from_list.py` keeps
  passing (it pins the *semantics*, which must not change: values are
  truncated to their low 8 bits rather than raising).
- `shedskin_examples/tonyjpegdecoder` completes its 20 iterations —
  see 045.
- Watch for a regression in `str.join`-heavy corpus programs; several
  build output that way.

## What this unblocks

045 outright. More broadly, any program that builds a string or a `bytes`
incrementally — which for a compiler targeting C is a shape users will
reasonably expect to be fast, and which is currently slower than CPython
by four orders of magnitude at realistic sizes.
