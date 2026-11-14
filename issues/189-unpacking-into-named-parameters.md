# 189 — unpacking into named parameters, and mixed `**kw` values

**Status:** open. Filed 2026-11-13 as the residual of
[182](closed/182-kwargs-and-keyword-only-parameters.md). Refusals, not
wrong answers. Steps 1 and 3 done 2026-11-14: `f(*t)` of a tuple binds
any parameters, and each refusal says what it refuses. Left: step 2,
`**d` into named parameters (the forwarding decorator) and mixed `**kw`
values.

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

1. **`*t` by arity, in a wrapper (done 2026-11-14).** The matcher
   (`pattern.cc`) treats the call's `*t` actual as one no candidate's
   formal types: every candidate admits it, nothing recurses into it, it
   ranks no candidate above another, and any positional formal may be
   left for it (`is_star_actual`, `match_star_at`, file state set for the
   one call being matched). `Matcher::build` then reads the tuple's arity
   from its CreationSets (`static_arity`; all must agree) and routes the
   call through `IFACallbacks::unpack_wrapper`, one per shape and arity.
   The wrapper takes the actuals as they are, reads the elements with
   `__getitem__`, and calls again with them in the tuple's place. That
   call is an ordinary one, so named parameters, defaults, keyword-only
   parameters and `*args` all bind by the existing rules, and a `**d`
   keeps its mark. A call already in the callee's own shape
   (`inner(x, *args, **kw)` forwarding, and every signature wrapper's own
   call) is not wrapped. The incremental matcher is bypassed for a `*t`
   call: a member's twin need not share its arity.

   Fixture: `tests/call_unpacking_into_named.py` (named parameters,
   defaults, a keyword after the star, `*args` surplus, `**kw` with none,
   keyword-only, mixed element types, a tuple from a list, a constructor
   and a method), the former `_refused` test.

2. **`**d` into named parameters: still open.** It needs the keys in
   the type. A `dict` doesn't have them, so `**kw` would become a record
   of named fields (`Sym::has_names` exists) with the dict protocol on
   top for `len`, `in` and iteration. Whether that touches CreationSet
   identity has not been checked. The same representation is what
   mixed `**kw` values need.

3. **Named refusals (done 2026-11-14).** The matcher records why it
   declined, per send (`unpacking_refusal_at`), and the violation prints
   it in place of "illegal call argument type":

   | case | message | fixture |
   | --- | --- | --- |
   | too many elements | CPython's `f() takes 2 positional arguments but 3 were given` | `call_unpacking_too_many_refused.py` |
   | a list, or tuples of different lengths | needs a tuple of one length known at compile time | `call_unpacking_list_into_named_refused.py` |
   | `f(**d)` into named parameters | `**` binds only a `**kw` parameter | `call_unpacking_dict_into_named_refused.py` |
   | the forwarding decorator | the same, at the decorated function, "called from" the `fn(*a, **kw)` | `call_unpacking_forwarding_decorator_refused.py` |

   Too few elements is the wrapper's own call failing, reported at the
   callee with the generic message. `print(*xs)` into a compiler-level
   builtin keeps its frontend refusal.

**Verified:** `make test` green: `test_pyc` 521 passed, 0 failed on both
backends. Corpus: `check__default__a3134327+c2ee613f` is identical per program
in every column, contour counts included, to
`check__default__328f0b12+ccf15f37`; only `circle` has a `*` call at all.
