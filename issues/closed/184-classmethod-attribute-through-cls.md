# 184 — an attribute read through `cls` in a `@classmethod` does not type

**Status:** closed 2026-11-18. FA's `P_prim_period` reads a class-valued
receiver's members through the class prototype. `cls.__name__` is split out
as [190](../190-class-name-attribute.md): no class has a `__name__`, by name
or through `cls`. Found 2026-10-16 while closing issues/181. A refusal, not a
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

## Root cause (confirmed)

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

## Resolution

As diagnosed. `cls` carries the class's META type (`add_var_constraint`
gives a class Sym `make_abstract_type(s->meta_type)`), whose CreationSet has
no members; `C.attr` by name reads `C->self`, the prototype global. In
`fa_prims.cc`'s `P_prim_period`, a receiver CreationSet whose sym
`is_meta_type` now reads the member from the CreationSets of its class's
prototype AVar (registered with `arg_of_send`, so the read re-runs as the
prototype's type grows). Each meta receiver reads its own class's prototype,
so `D.g(1)` with `g` inherited from `C` reads `D.base`. A function found
there is bound to the class value only when it is a `@classmethod`
(`Sym::is_class_method`); a plain method or `@staticmethod` flows unbound,
as `C.f` is the plain function in CPython (`cls.f(cls(), y)`). A meta
receiver answered this way is left out of the selector dispatch.

## Verification

`tests/classmethod_cls_attribute.py` passes on both backends (its
`.known_issue` removed; the `__name__` line moved to
`tests/class_name_attribute.py` for 190). It also covers a plain method and a
staticmethod read through `cls`.
