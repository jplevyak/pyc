# 184 — an attribute read through `cls` in a `@classmethod` does not type

**Status:** open. Found 2026-10-16 while closing issues/181. A refusal, not a
wrong answer.

## Symptom

```python
class C:
    base = 10
    @classmethod
    def g(cls, n):
        return cls.base + n
print(C.g(1))       # CPython: 11.   pyc: error: expression has no type
```

| inside the classmethod | pyc |
| --- | --- |
| `cls.base` (class attribute) | `expression has no type` |
| `cls.h()` (another classmethod) | `expression has no type` |
| `cls.__name__` | `expression has no type` |
| `cls(v)` (construct) | works |
| `C.base` (the class by name) | works |

Calling through an instance (`C().g(2)`) fails the same way. It does not
depend on generators: a `@classmethod` generator inherits it (issues/181).

## Root cause (to confirm)

A class-qualified call `C.g(...)` passes the class VALUE as `cls`
(`qual_cls`, `python_ifa_build_if1.cc`'s attribute-trailer path), and
`cls(...)` works through the ordinary `__new__`-through-meta dispatch. An
attribute read `cls.base`, though, is a period send on that class value, and
class attributes are reached through the class's PROTOTYPE (`C.base`
resolves the class Sym's `->self`), not through the meta value `cls`
carries. So the read has no type.

## Fix

Resolve a period send on a class value the way the frontend resolves
`C.attr` on a class name: read the prototype's member, and dispatch a
classmethod/staticmethod found there (`find_class_method_fn`). This belongs
in FA's `P_prim_period` for a meta-typed receiver, so that a `cls` bound to
several classes (`B.g()` inherited from `A`) reads each class's own member.

## Verification

`tests/classmethod_cls_attribute.py` (`.known_issue`) flips to PASS on both
backends.
