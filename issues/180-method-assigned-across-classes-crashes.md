# 180 — a bound method stored into another class's instance crashes when called

**Status:** open. Found 2026-10-16. The README listed it as a compile error
(its old "t30" limitation). It now compiles and SEGFAULTs, both backends.

## Symptom

```python
class A:
    def x(self): return 1
class B:
    def x(self): return 2
a = A(); b = B()
a.x = b.x
print(a.x())        # CPython: 2.   pyc: compiles clean, then segfaults
```

## Root cause

The store puts the bound-method closure for `b.x` into `a`'s member `x`
(typed `_CG_void`). The call `a.x()` is still emitted as a method-SLOT call on
`A`: it loads `a->x` and then calls through word `e16` of THAT value, which is
the closure struct rather than an `A`, as if it were a function pointer:

```c
t2 = (_CG_any)((_CG_ps19060)t3)->e16; /* x */
t1 = ((_CG_int64(*)(void*))((_CG_ps19060)(void*)t2)->e16)((void*)t2);
```

So one name is both a method of `A` and an instance field holding a closure,
and the call site treats the field's value as an `A`. A field holding a
closure has to be called as a closure. This is the same family as "is this
member a method slot?" answered without the call graph (AGENTS.md, "never
decide by name").

## Fix

At `a.x()`, when `a`'s member `x` can hold a closure value (FA's type for
the member), call that value; dispatch to `A.x` only for the instances whose
member is the class's method. If both are possible at one site, refuse
rather than guess.

## Verification

`tests/method_assigned_across_classes.py` (`.known_issue`) prints `2` on
both backends.
