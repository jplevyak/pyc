# 103 — an unrecognized keyword argument is silently bound to the next positional parameter

**Status:** open, root-caused 2026-08-16 while digging into
[ifa/issues/102](../ifa/issues/102-corpus-programs-compile-then-abort-at-runtime.md)
class A. Repro: `tests/unknown_kwarg_rejected.py` (`.known_issue`).
**Silent wrong behaviour, and the cause of at least one corpus program's
runtime abort.**

## Symptom

```python
def f(A, B=None, C=None):
    print(A)
    print(B is None)

f([1, 2], nosuchkw=99)
```

| | result |
|---|---|
| CPython | `TypeError: f() got an unexpected keyword argument 'nosuchkw'` |
| **pyc** | prints `[1, 2]` then **`False`** — `nosuchkw`'s value was bound to `B` |

No diagnostic at any stage. The program compiles and runs.

## Cause

`ifa/if1/pattern.cc`, `Matcher::build_positional_map`:

```cpp
MPosition *fcpp = f->named_to_positional.get(fcnp);   // null when f has no such formal
acpps_for_acnps.set_add(acpp);
fcpps_for_fcnps.set_add(fcpp);
m->actual_to_formal_position.put(acpp, fcpp);
```

When the callee has no formal with that name the lookup yields **null**,
and nothing treats that as a failed match. The actual is then not in
`acpps_for_acnps`, so the next loop — *"collect actual positions not used
by named arguments"* — sweeps it into `unused_acpps` and it is matched
**positionally**.

So a misspelled or unsupported keyword does not fail; it shifts an
argument into an unrelated parameter.

## How this reaches a runtime crash

`shedskin_examples/life` is the worked example, and it is
[102](../ifa/issues/102-corpus-programs-compile-then-abort-at-runtime.md)'s
cheapest class-A reproducer:

1. `life.py` calls `product((0, 1), repeat=rows*columns)`.
2. `pyc_lib/itertools.py` defines `product(A, B=None, C=None, D=None)` —
   **there is no `repeat` parameter**.
3. `repeat`'s value (an `int`) is silently bound to `B`.
4. `B is not None` now, so the body takes the `elif C is None:` branch and
   runs `for b in B:` — **iterating an integer**.
5. `int` has no `__iter__`, so the send has *no* candidate functions.
   `PYC_DBG_DISPATCH` shows it exactly:
   `DISPATCH FAIL in product: fns=-1 rvals=2 | r0=__iter__:symbol r1=_:int64`
6. FA types the result `void_type` (bottom); every downstream use is
   unresolvable, and `cg.cc:2055` emits
   `assert(!"runtime error: matching function not found")` rather than
   failing the build.
7. `life` compiles cleanly and aborts when the loop is entered.

All nine of `life`'s dispatch failures are class A, and this is the
upstream cause of them.

## Fix direction

Two parts, and the first is worth doing on its own:

1. **Reject the match.** In `build_positional_map`, a null `fcpp` means
   this candidate has no formal by that name — it is not a match. Making
   it fail turns a silent misbinding into a "no matching function"
   diagnostic, which is a large improvement even before anyone gets a
   proper error message. Note this area is delicate: the comment on
   `PycCompiler::order_wrapper` (`python_ifa_sym.cc`) records how
   [087](../ifa/issues/closed/087-DISPATCH-out-of-order-keyword-args.md)'s
   out-of-order keyword matching failed *far* downstream when a callback
   silently returned 0, which is the same failure mode as this one.
2. **Report it properly** — CPython's wording (`f() got an unexpected
   keyword argument 'nosuchkw'`) at the call site, which is what
   `tests/unknown_kwarg_rejected.py.check` currently pins.

Separately, `pyc_lib/itertools.py`'s `product` should support `repeat=`.
That is a real gap but it is the *second* bug here — with fix 1 in place
`life` would fail loudly at compile time instead of miscompiling.

## Verification plan

- `tests/unknown_kwarg_rejected.py` reports an error instead of printing
  `False`; delete its `.known_issue` tag.
- Re-run `ifa/issues/runstatus.sh`: `life`'s nine class-A dispatch
  failures should disappear (it will then fail to *compile* until
  `product(repeat=)` exists, which is the honest state).
- Check the corpus for other unknown-keyword calls before landing — this
  changes calls that currently "work" by accident, and some corpus
  programs may depend on the misbinding.

## What this unblocks

Part of [102](../ifa/issues/102-corpus-programs-compile-then-abort-at-runtime.md).
More importantly it closes a silent-wrong-answer hole: today any typo in
a keyword argument, or any call against a `pyc_lib` stub whose signature
has drifted from CPython's, binds a value into the wrong parameter with
no diagnostic at all.
