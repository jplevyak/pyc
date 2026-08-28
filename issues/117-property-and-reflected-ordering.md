# 117 — `property` (read-only subset) and reflected ordering

**Status:** partially implemented 2026-08-27. The read-only
`NAME = property(GETTER)` subset, Python's reflected ordering fallback
and `__list_iter__.__next__`'s StopIteration contract all work, and
voronoi2 COMPILES AND RUNS; the general descriptor protocol does not.
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

## The runtime crash: `__next__` never raised StopIteration

The crash after the compile fix was `Site::__lt__` with a NULL receiver,
from

```python
if (newsite and (priorityQ.isEmpty() or newsite < minpt)):
```

and it looked like an Optional-narrowing gap. It was not. `getnext` is

```python
def getnext(whatsit):
    try:    return whatsit.__next__()
    except StopIteration:  return None
```

and `__list_iter__.__next__` **never raised**:

```python
def __pyc_more__(self): return self.position < len(self.thelist)
def __next__(self):
    self.position += 1
    return self.thelist.__getitem__(self.position-1)
```

It relies on the `__pyc_more__` protocol that a for-loop calls first. A
bare `it.__next__()` past the end therefore indexed out of range instead
of raising, so the `except` branch was DEAD, `getnext`'s result was
typed `Site` (never `None`), `if newsite and ...` folded to always-true,
and the out-of-range read handed back a null Site. The
`__pyc_iterator__` bridge in `00_runtime.py` already documents exactly
this contract ("re-raises StopIteration past exhaustion like CPython");
`__list_iter__` simply did not honour it.

Fixed by raising when exhausted. **Write the guard inline, not as a
call**: the first version used `if not self.__pyc_more__()` and that
broke `min(3, 7)` -- `builtins_batch` grew "illegal call argument type"
warnings, because the extra METHOD CALL perturbed contour splitting
enough that FA stopped proving `min`'s `b is None` sequence branch dead.
`if self.position >= len(self.thelist)` keeps the raise and costs
nothing. The exception was never the problem; the call was.

### Corpus effect, measured compile AND run

Compile-only is not sufficient evidence for a change to every list
iteration, so both were swept, before and after:

    compiled     72 -> 72
    ran cleanly  20 -> 23

    score4           timeout -> RUNS
    tonyjpegdecoder  timeout -> RUNS
    voronoi2         segfault -> RUNS
    richards         timeout -> abort (see below)

score4 and tonyjpegdecoder were looping for ever off the end of an
iterator; the raise terminates them. richards changed from a 120 s
timeout to a fast abort on "polymorphic dispatch: no branch matched" --
it did not work before either (a benchmark that should finish in well
under a second was hanging), so this is hang -> fail-fast, not a
regression, but the dispatch gap it now exposes is real and unfiled.

## Not fixed

**The general descriptor protocol** -- a getter that computes, `@property`
decorator syntax, `@x.setter` -- is not implemented and would need the
type-directed rewrite in FA, as shedskin does.
