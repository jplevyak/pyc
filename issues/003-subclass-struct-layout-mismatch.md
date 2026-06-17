# Issue 003: Subclasses that redefine inherited fields produce mismatched C structs

**Status:** open.
**Affects:** `python_ifa_build_syms.cc` (`gen_class_pyda` —
class-attribute slot assignment); C backend (`ifa/codegen/cg.cc`
emits the broken struct accesses); v2 LLVM backend (same shape
emerges as LLVM `verifyModule` "Invalid indices for GEP pointer
type" failures).
**Related:** existing `tests/class_attr_mutation.py.python.expect_fail`
xfail (CPython-incompat already known); same root cause.

## Symptom

This compiles cleanly in CPython but fails in pyc on both
backends:

```python
class Shape:
  tag = 0
class Circle(Shape):
  tag = 1
class Square(Shape):
  tag = 2

items = [Circle(), Square(), Circle()]
for it in items:
  print(it.tag)
```

C backend (`./test_pyc`) produces:

```
poly.py.c:181:21: error: 'struct _CG_s2164' has no member
  named 'e0'; did you mean 'e1'?
  ((_CG_ps2164)t1)->e0 = (_CG_void)1;
                    ^~
                    e1
```

The generated C has parallel struct layouts for `Shape`,
`Circle`, and `Square`, but the field indices are inconsistent:
`Shape` uses `e0` for `tag`, while `Circle` and `Square` lay it
out differently (or vice versa), so an attribute store written
with one type's field index hits the wrong struct's body.

v2 LLVM backend produces the equivalent LLVM-side error:

```
fail: LLVM module verification failed: Invalid indices for GEP
  pointer type!
  %10 = getelementptr inbounds nuw ptr, ptr %it, i32 0, i32 0
```

— same root cause, just surfaced by `verifyModule` instead of
the C compiler.

## Why

Pyc's `gen_class_pyda` (`python_ifa_build_syms.cc:537`) lays
out each class's struct independently from its `has[]` list of
attribute Syms.  When a subclass redefines an inherited field
(`tag = 1` shadowing `Shape.tag = 0`), the subclass gets its
own struct with its own field index for `tag`, and there's no
guarantee that the index matches the parent's.

This is the same root cause as the existing
`tests/class_attr_mutation.py.python.expect_fail` xfail.  That
test was filed as "CPython-incompat" but the actual issue is
the static struct layout — runtime `A.n = 4` doesn't update
`B.n` because they're separate fields in separate structs.

Code pointers:
- `python_ifa_build_syms.cc:537` — `gen_class_pyda`.
- `python_ifa_build_syms.cc:613` — vector vs. regular class
  dispatch (the regular-class path is the one that needs the
  inheritance-aware layout).
- `ifa/analysis/clone.cc` — produces specialized AType per
  class; the per-class struct layout is consumed downstream by
  cg.cc.

## Proposed fix

Two paths, in increasing order of work:

1. **Match CPython semantics** — subclass struct layout must
   start with the same prefix as the base class.  When emitting
   the subclass struct, copy the base's field-order for
   inherited fields and append new fields at the end.  Reads
   through a base-typed pointer then GEP into the same indices
   regardless of the dynamic type.
2. **Drop the static struct optimization for inheritance** —
   use vtable-style indirection for attribute access on classes
   with subclasses, similar to how Python actually does it.
   More work but unblocks the full set of Python OO patterns.

Path 1 is enough to make the test above pass and to retire the
existing `class_attr_mutation` xfail.  Path 2 is needed if pyc
ever wants to support full MRO / `super()` dispatch.

## Verification plan

1. Reproduce: the program above fails both backends on
   current `main` (`b24bfbb`).
2. Land the fix.
3. Re-add `tests/polymorphic_list.py` (dropped in `b24bfbb`)
   with `.exec.check` containing `1\n2\n1\n`:

   ```python
   class Shape:
     tag = 0
   class Circle(Shape):
     tag = 1
   class Square(Shape):
     tag = 2
   items = [Circle(), Square(), Circle()]
   for it in items:
     print(it.tag)
   ```
4. Convert `tests/class_attr_mutation.py.python.expect_fail`
   into a real `.exec.check` once the static struct issue is
   fixed.
5. Both backends must pass:
   - `./test_pyc -k polymorphic_list -k class_attr_mutation`
   - `IFA_LLVM_V2=1 PYC_FLAGS=-b ./test_pyc -k polymorphic_list -k class_attr_mutation`

## What this unblocks

- Real Python class hierarchies — sub-classing with field
  override is the most common OO pattern.
- The retirement of the long-standing
  `class_attr_mutation.py.python.expect_fail` xfail.
- Polymorphic containers (lists / dicts of base-typed instances
  populated with subclasses), the test we tried to add for the
  v2 LLVM opaque-ptr coverage.
- Method override / `super()` calls (path 2 only).
