# 182 — `**kwargs` and keyword-only parameters

**Status:** closed 2026-11-13. Keyword-only parameters, `**kw` parameters,
named parameters after `*args`, and call-site `f(*t)` / `f(**d)` into a
`*args` / `**kw` callee all work, on both backends and under `--strict`.
What is still refused is in
[189](../189-unpacking-into-named-parameters.md). Found 2026-10-16 while
checking the README against the tree.

## What was wrong

```python
def f(**kw):
    return kw["x"] + len(kw)
print(f(x=3, y=4))      # CPython: 5.   pyc: illegal call argument type

def g(a, b=2, *, c=3):
    return a + b + c
print(g(1, c=5))        # CPython: 8.   pyc: syntax error after '*'
```

Three more turned up on the way, none of them refusals that named a cause:

- `f(*t)` and `f(**d)` at a call were parsed with the `*` dropped, so the
  tuple was passed as ONE positional argument. `def f(*a): return len(a)`
  with `f(*(1, 2))` printed 1. Silent wrong answer.
- A generator could not be called with a keyword argument or have `*args`:
  its public wrapper's formals had no names and no kinds.
- Two overrides that both take `*args`, called with surplus arguments,
  crashed the compiler in `subsumes_arg` (a surplus actual has no formal).

## How it works

**Nothing in `ifa/analysis/` changed.** The binding is done by a wrapper
function the frontend generates, exactly as `*args` already was, and flow
analysis sees ordinary code: a tuple construction, a dict literal with
`str` keys, a call.

- **Grammar** (`python.g`): a bare `*` is a `PY_star_arg` with no child,
  parameters may follow `*` / `*args`, and a call's `*t` / `**d` are
  marked (`PY_star_arg` / `PY_dstar_arg` in the `PY_arglist`).
- **Formals** stay in source order. `get_syms_args_pyda` records which are
  `*args`, keyword-only and `**kw`; `finalize_functions` turns that into
  `Fun::rest_pos`, `kwonly_pos`, `kw_pos`. Default expressions are keyed
  by the position of their own parameter (they were assumed to be the
  tail, which `def f(a, b=2, *args)` already broke).
- **A function with only keyword-only parameters** is matched as before:
  named binding, `default_wrapper`, `order_wrapper`. The matcher adds one
  rule, that a positional actual does not bind a formal from `kwonly_pos`
  on. The wrappers that forward to such a function pass those by name.
- **A function with `*args` or `**kw`** has every call routed
  (`Matcher::build`) through `IFACallbacks::signature_wrapper(f,
  npositional, keywords)`, cached per call shape. It replaces
  `rest_wrapper`. The wrapper takes exactly the call's actuals and binds
  them as CPython does: positionals to the plain formals, the surplus into
  a tuple, each keyword to the formal of that name, the rest into a dict,
  defaults for what is left. It then calls the function with one
  positional actual per formal. The tuple and the dict are marked
  `Sym::is_rest`, which is how the matcher tells that call from one still
  to be wrapped. A call CPython raises `TypeError` for has no wrapper, so
  it has no match.
- **`f(*t)` and `f(**d)`** pass one actual each, `t.__pyc_star_args__()`
  (a tuple is itself, a list becomes a tuple) and `d.__pyc_star_kwargs__()`
  (a copy), marked `is_rest`. A marked actual binds the `*args` / `**kw`
  formal and nothing else, so a forwarding call is the direct call above
  and needs no wrapper.
- **Constructors and generators**: the `__new__` wrapper and the generator
  wrapper mirror the names and kinds of the formals they forward to.

`**kw` is a `dict[str, T]`. Calls of one shape with different value types
get separate contours by the ordinary splitting (`f(x=1)` and `f(x="a")`
both work); one call with two value types is a mixed dict and is refused
like any other.

## Verification

`make test` green: 516 passed, 0 failed on both backends.

- `tests/kwargs_parameter.py` (its `.known_issue` removed),
  `keyword_only_parameter.py`, `call_unpacking.py`,
  `kwargs_generator_lambda_decorator.py`: CPython's output.
- Refusals, each a call CPython raises `TypeError` for or one 189 covers:
  `keyword_only_positional_refused.py`, `keyword_only_missing_refused.py`,
  `kwargs_duplicate_refused.py`, `call_unpacking_into_named_refused.py`.
- The four positive tests also match CPython under `--strict`.

Corpus `check` sweep `check__default__328f0b12+d7246212`: 77 of 77 compile,
0 run failures, 1 stdout difference (`othello2`, which is
[177](../177-int64-overflow-at-run-time-is-silent.md) and differed before).
No verdict changed against `check__default__eb635d2a`.
