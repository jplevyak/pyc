# issues/162 — dict insertion does not scale

**Status:** open, measured 2026-09-17.

## Symptom

`shedskin_examples/dijkstra2` compiles (ifa/160) and then takes
asymptotically longer than CPython:

| n | pyc | CPython | ratio |
| --- | --- | --- | --- |
| 20 | 4.23 s | 0.15 s | 28x |
| 50 | 170.65 s | 1.04 s | **164x** |
| 100 | >240 s (timeout) | 5.54 s | — |

The ratio GROWS with n, so this is not a constant-factor gap.

## Reduced

Inserting `n` integer keys into a dict, nothing else:

```python
d = {}
for i in range(n):
    d[i] = i
```

| n | pyc | CPython |
| --- | --- | --- |
| 5 000 | 0.00 s | 0.00 s |
| 10 000 | 0.00 s | 0.00 s |
| 20 000 | **4.00 s** | 0.00 s |

(`time.time()` resolves to whole seconds under pyc, so the small values are
only "under a second"; the 20 000 point is the finding.)

Four seconds to insert twenty thousand integer keys, against about a
millisecond in CPython, is consistent with quadratic insertion — a missing or
mis-sized rehash, or a linear probe over a table that never grows.

`dijkstra2` is dict-heavy (`self.vertices`, `seen[dir]`, `paths[dir]`, all keyed
by `(x, y)` tuples), which is why it shows the asymptotics so clearly.

## Why it matters beyond one program

It is invisible to every existing gate. The suite's programs are small, and the
corpus sweep's 120 s cap records a timeout as `rc=124` without saying that the
cause is complexity rather than a hang. Nothing in the harness measures scaling.

## Next

Read the `dict` implementation in `__pyc__/07_dict.py` and the runtime it lowers
to, and check the growth policy: whether the table resizes at all, and on what
load factor. The reduced case above is the test.
