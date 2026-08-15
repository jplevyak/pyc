# 046 — two call sites omitting *different* defaulted parameters: the defaulted store is lost (silent wrong answer)

**Status:** open, 2026-08-15. Root-caused from `shedskin_examples/sat`'s
runtime failure. Minimal repro landed as
`tests/default_arg_omitted_differently.py`
(+ `.python.expect_fail`). **This is a wrong-answer miscompile, not a
diagnostic** — pyc compiles cleanly except for one warning and then
prints a different number than CPython.

## Symptom

```python
class Clause:
    def why(self): return 7

class VarInfo:
    def __init__(self):
        self.reason = None
        self.reason_txt = None

class Solver:
    def __init__(self):        self.v = VarInfo()
    def enqueue(self, reason=None, reason_txt=None):
        self.v.reason = reason
        self.v.reason_txt = reason_txt
    def run(self):
        self.enqueue(reason=Clause())      # omits reason_txt
        self.enqueue(reason_txt="learnt")  # omits reason   <- DIFFERENT omission
        c = self.v.reason
        if c: return c.why()
        return 0

print(Solver().run())
```

CPython prints **0** — the second call resets `reason` to its default
`None`, so `c` is falsy. pyc prints **7**: the second call never wrote the
default, so `self.v.reason` still holds the first call's `Clause`.

The one diagnostic emitted is a *consequence*, not the bug:

```
warning: illegal call argument type 'c' illegal: str
```

i.e. `self.v.reason` has also acquired `str` from the other field.

## Narrowing (all measured)

| variant | result |
|---|---|
| as above | **miscompiles** (0 → 7) |
| without `__slots__` | **miscompiles** — not slots-related |
| both args passed explicitly (`enqueue(Clause(), None)`) | clean |
| both call sites omit the **same** parameter | clean |
| one call passes both, the other omits one | clean |
| only ever omitting the **second** parameter | clean |
| distinct non-`None` defaults | 1 warning; answer coincidentally matches |

**Trigger: two call sites of the same function that omit _different_
defaulted parameters.** Not `__slots__`, not `None`-as-the-default, and
not keyword syntax as such — `sameomit` uses the same keyword syntax and
is clean.

## Why it matters / what it broke

`shedskin_examples/sat` has exactly this shape:

```python
def enqueue(self, lit, reason=None, reason_txt=None):
    ...
    var_info.reason = reason
    var_info.reason_txt = reason_txt
```

with call sites passing `reason=<Clause>` at some points and
`reason_txt="considering"` / `reason_txt=reason` at others (sat.py:480,
496). The result is `cause = var_info.reason` typed `Clause ∪ str`
(sat.py:373/377 warn `illegal call argument type 'cause' illegal: str`),
and at run time sat dies with an unhandled `AssertionError` — one of its
many `assert`s fails because the solver's bookkeeping is wrong.

`sat` began compiling only after the 2026-08-15 divergence-guard fix
(ifa/issues/074), which is how this surfaced; the defect itself is
independent of and older than that change.

## Where to look

The lowering of a call that omits defaulted parameters — pyc creates a
separate `Fun` per keyword/arity shape (two distinct `min` symbols are
visible in generated C for a 2-arg and a 3-arg call), so the suspicion is
that the *store of the default* is attached to the wrong shape, or elided
when another shape supplies that position. `python_ifa_build_if1.cc`'s
call construction and the partial-application path are the places to
start.

## Verification plan

- `tests/default_arg_omitted_differently.py` must print `0`, matching
  CPython, and its `.python.expect_fail` must then be deleted.
- `sat` should lose the two `'cause' illegal: str` warnings.
- Re-run the shedskin sweep: `sat` currently compiles and dies with
  "Unhandled exception"; it should run.

## What this unblocks

`sat`. More importantly it removes a **silent** wrong-answer class from
ordinary Python — defaulted keyword parameters used non-uniformly across
call sites is an extremely common shape, and today it produces no error,
just a different answer.
