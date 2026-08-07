# Issue 031: `x == None` / `x != None` dispatch through the generic
`__eq__`/`__ne__` method, crashing at runtime for container-typed `x`

**Status:** fixed 2026-07-28.

**Affects:** pyc Python frontend (`python_ifa_build_if1.cc`'s
`PY_compare` lowering).
**Surfaced while:** digging into
`shedskin_examples/chaos/chaos.py`'s runtime crash.

## Symptom

`shedskin_examples/chaos/chaos.py` compiled cleanly (only warnings,
exit 0) but aborted immediately at runtime:

```
chaos: chaos.py.c:8948: _CG_bool _CG_f_2377_207(_CG_ps13411, _CG_nil_type):
  Assertion `!"runtime error: getter not resolved"' failed.
```

With `-r` (strict typing), the same site fails to compile:

```
chaos.py:60:26: error: unresolved call '__ne__'
            if knots == None:
                             ^
chaos.py:60:26: error: illegal call argument type 'i' illegal: int64
chaos.py:60:26: error: expression has no type
fail: program does not type
```

Minimal repro:

```python
def describe(knots):
    if knots == None:
        return "none"
    return "list:" + str(len(knots))

print(describe([1, 2, 3]))
```

## Root cause

`Spline.__init__(self, points, degree=3, knots=None)` gives `knots`
type `{None, list[int]}` (default `None`, callers also pass a real
list). `if knots == None:` uses `PY_CMP_EQ`, which
`map_pyop_to_cmp` (`python_ifa_build_if1.cc:143`) maps to a bare
`__eq__` selector send — unlike `is None`/`is not None`
(`PY_CMP_IS`/`PY_CMP_IS_NOT`), which issues 024/025 already special-
case to lower directly to `prim_isinstance(operand, sym_nil_type)`
instead of a method dispatch.

For the `list` arm of the union, this sends `list.__eq__(knots_list,
None)`. `list.__eq__` (`__pyc__/04_sequence.py`) is written assuming
its argument is another list:

```python
def __eq__(self, l):
    ll = __pyc_clone_constants__(len(l))   # len(None) here
    ...
    for i in range(lself):
        if l[i] != self[i]:                # None[i] here
            ...
```

IFA monomorphizes `list.__eq__` for the call-site argument type
(`nil`), producing a specialization whose body still calls `len(l)`
and indexes `l[i]` on a nil-typed `l` — codegen degrades those to
runtime traps (`getter not resolved`) rather than compiling
nonsense C, per the issue-063 convention, so the crash is a *correct
degradation of a wrong dispatch decision*, not a separate codegen
bug. The `__ne__` name in the error output (rather than `__eq__`)
comes from a downstream site: `list.__eq__`'s own `l[i] !=
self[i]` element comparison.

Real Python doesn't have this problem because `list.__eq__` returns
`NotImplemented` for a non-list operand and the interpreter falls
back to identity comparison (`False`, since `None` is never a list).
For every builtin/user class in this corpus (none override `__eq__`
to special-case `None`), `x == None` and `x is None` already agree
in outcome — `is None` just gets there without a method dispatch.

## Fix

Extended the same-operand-is-a-None-literal detection
(`lv->rval == sym_nil` / `rv->rval == sym_nil`) that issues 024/025
already use for `PY_CMP_IS`/`PY_CMP_IS_NOT` to also cover
`PY_CMP_EQ`/`PY_CMP_NE`, in `python_ifa_build_if1.cc`'s `PY_compare`
case (`n_pairs == 1` branch):

- `x == None` / `None == x` → `isinstance(x, __pyc_None_type__)`
- `x != None` / `None != x` → `not isinstance(x, __pyc_None_type__)`
- `None == None` → `True`, `None != None` → `False` (constant fold)

Ordinary `==`/`!=` between two non-None-literal operands is
untouched — still routes through the normal `__eq__`/`__ne__`
dispatch (needed for e.g. two `Vertex` instances). The `is`/`is not`
real-identity path (`else if (op == PY_CMP_IS || op ==
PY_CMP_IS_NOT)`, non-None operands) is also untouched.

## Verification plan

1. `chaos.py` compiles and runs to completion instead of aborting:
   `./pyc shedskin_examples/chaos/chaos.py && ./shedskin_examples/chaos/chaos`
   completes (`TIME ...` printed), no assertion.
2. New regression test `tests/eq_none.py` (list/None, object/None,
   None/None, both operand orders, both `==` and `!=`) — output
   cross-verified against CPython 3, both backends.
3. Suite stays green: `python3 test_pyc.py` and `PYC_FLAGS=-b
   python3 test_pyc.py` — 230/0/6/4 both backends (was 229/0/6/4;
   +1 for the new test).

## What this unblocks

- `x == None` / `x != None` idioms against container-typed
  (`list`/`dict`/`set`/`tuple`) or plain-object values no longer
  crash — a pattern that appears 8 more times across the shedskin
  corpus (`kanoodle`, `minilight`, `quameon`, `go`, `yopyra`,
  `webserver`), though most of those examples have other, unrelated
  blockers still open.
- `shedskin_examples/chaos/chaos.py` now runs correctly end-to-end
  (10 iterations of a 2000×2000 chaos-game fractal render).

## Related

- [ifa/issues/closed/024](../../ifa/issues/closed/024-is-comparison-narrowing.md)
  and [closed/004](closed/004-is-operator-unimplemented.md) — the
  `is`/`is not` → `prim_isinstance`/`prim_is` lowering this fix
  mirrors.
- [ifa/issues/063](../../ifa/issues/closed/063-no-type-bucket-triage.md) —
  the degrade-to-runtime-trap convention that turned this into a
  clean assertion instead of miscompiled C, and the broader
  "unattributed cascade" bucket `chaos` was filed under
  (`issues/025-shedskin-examples-coverage.md`'s R4) before this dig
  pinned down its specific root cause.
