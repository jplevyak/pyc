# 182 — `**kwargs` and keyword-only parameters are not supported

**Status:** open. Found 2026-10-16 while checking the README against the
tree. Refusals, not wrong answers.

## Symptom

```python
def f(**kw):
    return kw["x"] + len(kw)
print(f(x=3, y=4))      # CPython: 5.
                        # pyc: error: illegal call argument type expression illegal: int64

def g(a, b=2, *, c=3):
    return a + b + c
print(g(1, c=5))        # CPython: 8.   pyc: syntax error after '*'
```

`*args` works.

## Root cause (to confirm)

- `**kw`: the grammar accepts the parameter (`dstar_arg`), but keyword
  arguments that match no named formal are not collected into a dict for it.
  The call fails to type, and the message names neither the parameter nor
  the cause.
- Keyword-only parameters: `python.g` has no production for a bare `*`
  separator in a parameter list. A `*args` followed by named parameters
  should be checked too.

## Fix

- Parse a bare `*`, and bind the parameters after it by keyword only, with
  their defaults (a missing one is CPython's `TypeError`).
- Collect the unmatched keyword arguments of a call into a `dict` for a
  `**kw` formal. With values of one type that is an ordinary `dict[str, T]`.
  With several value types it is a mixed dict, which is refused like any
  other union. `f(**d)` at a call site is the matching half.
- Until then, refuse `**kw` with a diagnostic that names it.

## Verification

`tests/kwargs_parameter.py` (`.known_issue`) flips to PASS on both
backends. The keyword-only repro above becomes `tests/keyword_only_parameter.py`
with the grammar fix. It cannot be added before then, because `make
test-dparse` requires every test file to parse wherever CPython parses it.
