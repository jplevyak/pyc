# 117 — `property` (read-only subset) and reflected ordering

**Status:** partially implemented 2026-08-27. The read-only
`NAME = property(GETTER)` subset and Python's reflected ordering
fallback both work; the general descriptor protocol does not.
**Affects:** `python_ifa_build_syms.cc` (`rewrite_class_properties`),
`__pyc__/00_runtime.py` (`object.__gt__` / `object.__ge__`).
**Found:** blocking `shedskin_examples/voronoi2`, the last corpus
program rejected for a missing FEATURE rather than an analysis bug.

## What was missing

`voronoi2` uses

```python
def _getxmin(self): return self.__xmin
xmin = property(_getxmin)
```

and `property` was simply on `is_unimplemented_builtin`'s list.

## Why the general feature is type-directed

Python's `property` is a DESCRIPTOR: `obj.NAME` calls `GETTER(obj)`.
Whether an attribute read is a call or a field read therefore depends on
the receiver's CLASS -- and voronoi2 has both in ONE program: `xmin` is a
property on `SiteList` and a plain instance field on `EdgeList` and
`PriorityQueue`. So no frontend-only, name-based rewrite can be correct.

shedskin does exactly the type-directed rewrite. Running it on the same
file:

```cpp
edgeList = (new EdgeList(siteList->_getxmin(), siteList->_getxmax(), ...));
...
bucket = __int((((pt->_x - this->xmin) / this->deltax) * this->hashsize));
```

a getter CALL for the property, a plain field for the field.

The cheap shortcut is also closed off: pyc supports bare bound-method
references (`f = c.get; f()` works), so "auto-call a function-valued
attribute read" would break that.

## What was implemented

**The read-only trivial-getter subset**, in the frontend, with no FA
change. When the getter's whole body is `return self.ATTR`, the property
is nothing but a public name for a private field -- so
`rewrite_class_properties` renames `ATTR` to the property's name inside
the class and drops the assignment. `obj.NAME` is then an ordinary field
read, which FA and codegen already resolve per class, and the collision
above resolves itself because fields are per-class.

Runs in the `PY_classdef` case BEFORE anything binds the body. Anything
less trivial is left alone and still reports "builtin 'property' is not
supported by pyc" rather than being silently mis-lowered.

**Reflected ordering.** voronoi2's `Halfedge` defines only `__lt__` but
the code says `he > next`. Python falls back to `b.__lt__(a)`; pyc's
frontend lowers `>` straight to a `__gt__` call
(`python_ifa_build_if1.cc`, `PY_CMP_GT`), so it aborted at runtime with
"matching function not found". `object` now defines

```python
def __gt__(self, x): return x.__lt__(self)
def __ge__(self, x): return x.__le__(self)
```

Putting it on `object` rather than in the dispatcher gets resolution
order right for free: a class with its own `__gt__` overrides it, one
with only `__lt__` inherits the reflection. Deliberately only this
direction -- reflecting `__lt__` to `__gt__` as well would make a class
defining NEITHER recurse for ever, where today it reports an unresolved
call (CPython raises TypeError, so nothing correct is lost).

## Result

    voronoi2   rejected at name resolution -> COMPILES (corpus 71 -> 72)
    suite      299 -> 300 passed, 0 failed, 13 known, both backends

`tests/property_readonly.py` covers both features, including the
same-name property/field collision.

One re-bless: `tests/minmax_3arg.py.check`. Adding 21 lines to
`00_runtime.py` shifted the `__pyc__.py` line numbers it records and
reordered two warnings; the warning SET is byte-identical, verified
before re-blessing.

## Not fixed

**voronoi2 still crashes at runtime**, now in `Site::__lt__` with a NULL
receiver. The guard is

```python
if (newsite and (priorityQ.isEmpty() or newsite < minpt)):
```

and `getnext()` returns `Site | None`; pyc does not narrow the Optional
at the `and`, so it emits the comparison with a possibly-null receiver.
That is the Optional-without-narrowing gap (the same one
`prim_period_offset`'s nil_type comment describes), not this issue.

**The general descriptor protocol** -- a getter that computes, `@property`
decorator syntax, `@x.setter` -- is not implemented and would need the
type-directed rewrite in FA, as shedskin does.
