# 172 — a generator's `send` channel is `int64`, and `StopIteration.value` reads the global exception slot

**Status:** open. Split out of
[171](closed/171-permissive-accommodations-must-be-flagged-and-non-strict.md) #7 on
2026-09-29. The RETURN value half of #7 is done (its own channel,
`gen_retcell`); these are the two halves that need a real channel.

## 1. The `send` channel is `int64`

```python
def g():
    x = yield 1
    print(x)
    yield 2
it = g()
print(next(it))
print(next(it))     # CPython: None, then 2.   pyc (permissive): 0, then 2
```

`P_prim_yield` (`ifa/analysis/fa_prims.cc`) types a yield expression's
value as `int64`, because the value arrives through the C++ promise's `sent`
field. No IF1 edge connects `__pyc_generator__.send(value)` to the
primitive. So:

- a resume by `next()` reads `0` where CPython gives `None`;
- a non-int `send()` is refused (`_CG_generator_send` takes `int`).

**Gated 2026-09-29, not fixed.** It is an accommodation, so it is now
flagged: permissive warns at every yield whose value is used
(`PYC_YIELD_INT_SEND`, default on in permissive). `--strict`, or
`PYC_YIELD_INT_SEND=0`, refuses the program. Five suite tests carry the
warning in their `.check`: `generator_basic`, `generator_methods`,
`generator_infinite`, `generator_yield_from` and `generator_return_value`.

**Fix.** Give the send value a channel the way the return value got one: a
per-generator cell on the `__pyc_generator__` object, written by `send()`
(and by `__next__` with `None`), read by the yield primitive. The primitive's
result then flows from that cell's AVar, so it is typed per generator
instead of pinned to `int64`. `yield from` already forwards `send` through
the sub-generator's `send()`, so it needs no separate change. The runtime
hand-off (`_CG_generator_send`) keeps passing an opaque word for the resume
itself; only the TYPE moves into the cell.

## 2. `StopIteration.value` reads the one global exception slot

`except StopIteration as e: e.value` reads `__pyc_exc__`, which unions
every `StopIteration` raised anywhere in the program. With two value types
it is refused: `tests/stopiteration_value_exception_slot.py`
(`.known_issue`) holds `{None, int64, str}`. The CreationSets are already
separated by value type; the confluence is the slot. Refused, not silent.

**Fix.** Exception flow along the call graph: which raises can reach which
handler. That is a general FA change (the slot is a global), not a generator
one. `yield from` does not need it: it reads the sub-generator's own
`__pyc_return_value__()`.

## Verification

- The repro prints `None` then `2`, under `--strict` too, and the
  `PYC_YIELD_INT_SEND` gate and its warnings are deleted.
- `x = yield` receiving a `str` from `send("a")` compiles and prints it.
- `tests/stopiteration_value_exception_slot.py` flips to PASS.
