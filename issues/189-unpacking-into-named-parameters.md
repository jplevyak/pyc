# 189 — unpacking into named parameters, and mixed `**kw` values

**Status:** open. Filed 2026-11-13 as the residual of
[182](closed/182-kwargs-and-keyword-only-parameters.md). Refusals, not
wrong answers.

## Symptom

```python
def f(a, b):
    return a - b
t = (5, 2)
print(f(*t))            # CPython: 3.   pyc: illegal call argument type
print(f(**{"a": 1, "b": 2}))            # same

def log(fn):
    def inner(*a, **kw):
        return fn(*a, **kw)     # refused when fn has named parameters
    return inner

def g(**kw): return len(kw)
print(g(x=1, y="s"))    # CPython: 2.   pyc: refused, a mixed dict
```

Also refused: a positional or keyword argument combined with unpacking
where the unpacked value is not the callee's whole `*args` / `**kw`
(`f(1, *t)` when `f` has a second named parameter, `f(x=1, **d)`), and
unpacking into a compiler-level builtin (`print(*xs)`), which names the
cause.

The diagnostics for the first group are the generic `illegal call argument
type`. They should say what CPython's `TypeError` says, or name this issue.

## Root cause

182 binds a call by its SHAPE: how many positionals, which keywords. A
`*t` or `**d` actual is passed whole and binds only the callee's `*args`
or `**kw` formal. Binding it to named parameters needs more in the shape:

- `*t` into named parameters needs the tuple's arity. A tuple CreationSet
  has one (`static_arity`), so the signature wrapper can be keyed on it
  and index the tuple. The matcher has the actual's type in hand when it
  builds the wrapper. Probably no analysis change.
- `**d` into named parameters, the forwarding decorator, and mixed value
  types all need the KEYS in the type, which a `dict` does not have. That
  means representing `**kw` as a record of named fields (`Sym::has_names`
  exists), with the dict protocol on top for `len`, `in` and iteration.
  Whether that touches CreationSet identity is not checked.

## Fix

1. `*t` by arity, in the wrapper. Verification: `f(*t)` above, and
   `tests/call_unpacking_into_named_refused.py` flips to a passing test.
2. Decide the record representation for `**kw` and measure what it costs.
3. Until then, give the refusals their own message.
