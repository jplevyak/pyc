# 172 — a generator's `send` channel is `int64`, and `StopIteration.value` reads the global exception slot

**Status:** open: #1 FIXED 2026-10-16 (see its Resolution); #2 open. Split out of
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

### Resolution of #1 (2026-10-16)

The fix as proposed: `__pyc_generator__` has a `sendval` cell, written by
`send(v)` and by `next()` before they resume the body, and `x = yield`
reads it through the body's hidden generator-object formal (the one
`retval` uses). The value is typed per generator. The repro prints `None`
then `2` in every mode, `--strict` included, and `send("a")`, an object and a
tuple all arrive. The coroutine's `sent` word is no longer read, and
`_CG_generator_send` is deleted (`send` resumes with `_CG_generator_advance`).
`PYC_YIELD_INT_SEND` and its warning are deleted. A generator with no
generator object (only a `@staticmethod` generator, which is not supported
at all yet) is refused with a named error rather than given the int channel.

**One accommodation remains, narrower, and it is named.** The priming
`next()` of the common `next(g); g.send(5)` idiom stores a None that CPython
never delivers (no yield is paused yet). Field types are flow-insensitive,
so `x` types as `{None, int}`, which has no representation: `total += x`
failed to compile in every mode. Under permissive only
(`PYC_SEND_NONE_ZERO`, default on), a generator whose cell converged to
`{None, scalar}` is recorded by `reanalyze` (`pyc_record_send_none_cells`,
`python_ifa_main.cc`). On the next pass its `next()` resume delivers a zero
of the cell's own type (`__pyc_resume_none__`), so a non-priming `next()`
reads 0 where CPython gives None. It warns at the generator's `def`.
`--strict` gives a note at the `def`. **Correction (2026-10-16):** it does not always refuse. `total += x` is refused, but `x + 5` on the `{None, int}` value compiles under `--strict` and reads None as 0. That was the general operator gap of [183](closed/183-none-or-int-in-arithmetic-reads-none-as-zero.md), fixed the same day: `--strict` now refuses it. The
record is a decision kept across passes, so the transfer stays monotone
within one. The five suite tests carry the new warning and match CPython.
The precise answer is a started/unstarted typestate on the generator object,
not built.

**`yield from` starts the sub-generator with `next()`**, as PEP 380 does, not
`send(None)`. A send stores into the sub-generator's cell, so `send(None)`
put a None there through a path the record does not cover.

**A latent FA bug, found on the way.** `provably_constant_isinstance`
(`python_ifa_sym.cc`) folds an exception check to "cannot raise" when every
call on the check's AST cannot. It read the contour's `out_edges`, which
grow during a pass. With two calls on one AST (the new `yield from`: a
`__next__` and a `send`), it saw only the `__pyc_to_bool__` edge analyzed
first and folded the check after `send`. Nothing re-ran it, so the
sub-generator's StopIteration went unhandled (`generator_methods`). It now
walks the AST's own call PNodes, which are static, and declines while any has
no resolved edge in the contour.

`tests/generator_send_typed.py` pins the exact cases (a `next()`-only
resume reads None; str, object and tuple sends; a str through `yield
from`), byte-identical to CPython on both backends and under `--strict`
with no warning. `make test`: 474 passed, 13 known, 0 failed on both
backends. Sweep `check__default__0e65aa3e+59ad4f41` (`-R`): every corpus compile, warning and stdout verdict unchanged, +20 EntrySets / +10 CreationSets; no corpus program uses generator `send()`.

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
