# Issue 010: Multiple inheritance from unrelated base classes fails to compile

**Status:** open.
**Affects:** `python_ifa_build_syms.cc:294-298` (`PY_classdef` —
calls `Sym::inherits_add` once per base); `ifa/if1/sym.cc:330-334`
(`inherits_add` — populates `includes`/`implements`/`specializes`);
downstream FA/codegen type resolution for the class prototype
(exact fault site not yet traced — see below).
**Related:** `issues/003-subclass-struct-layout-mismatch.md` is a
different bug in the same neighborhood (single inheritance,
subclass redefines an inherited field) — this issue is about two
*unrelated* base classes with disjoint fields, no redefinition
involved.

## Symptom

A class with two independent (non-overlapping-hierarchy) base
classes fails to produce working code:

```python
class A:
    def f(self):
        return 1
class B:
    def g(self):
        return 2
class C(A, B):
    pass
c = C()
print(c.f())
print(c.g())
```

```
multi_inherit.py:10: illegal call argument type expression illegal:
multi_inherit.py:10: expression has no type
...
multi_inherit.py.c:48:18: error: '_CG_void' {aka 'void*'} is not a
  pointer-to-object type
   48 |   t1 = (_CG_void)_CG_prim_clone_dst(_CG_void, /* 3315 */ g1);
fail: compilation failure
```

pyc exits non-zero (the failure is at least *caught*, unlike
issues 006-009), but the diagnostics are confusing type-inference
noise rather than a clean "multiple inheritance unsupported"
message, and the eventual failure is a downstream C compile error
rather than a pyc-level one.

## Root cause (partially traced)

`build_syms_pyda`'s `PY_classdef` case (`python_ifa_build_syms.cc:294-298`)
calls `cdef_ast->sym->inherits_add(base)` once per base class in
the `class C(A, B):` header — so both `A` and `B` are correctly
appended to `C`'s `includes`/`implements`/`specializes` lists
(`Sym::inherits_add`, `ifa/if1/sym.cc:330-334`, is itself
base-count-agnostic; it just does three `.add()` calls).

`gen_class_pyda`'s prototype-init loop (`python_ifa_build_syms.cc:576-591`)
also iterates `cls->includes.n` generically, so at the frontend
level nothing obviously assumes a single base.

The failure surfaces later: the generated C shows
`_CG_prim_clone_dst(_CG_void, ...)` — the class prototype's
element type resolved to `_CG_void` (i.e., FA never pinned down a
concrete instantiated type for `C`'s prototype) rather than `C`'s
struct type. This smells like an FA/dispatch-level issue in how a
class Sym with **two** independent `includes` entries gets
resolved to a concrete type during specialization, rather than a
pyc-frontend bug — but the exact fault site (FA's clone/dispatch
logic, or the C-backend struct-layout code in `cg.cc`) hasn't been
traced past this point. Flagging here because the symptom is
Python-source-visible; may need to be split into an `ifa/issues/`
companion once the FA-side mechanism is identified.

## Proposed fix sketch

1. Add targeted `IFA_LOG_FLAGS` tracing (`-l` flags, see
   `ifa/AGENTS.md`) around class-prototype specialization for the
   `C(A, B)` repro to find where the concrete type resolution to
   `_CG_void` happens.
2. Once traced: either (a) the FA-level dispatch/clone machinery
   needs to handle a Sym with 2+ `includes` entries whose field
   sets are disjoint (no diamond, no redefinition — the simplest
   multiple-inheritance case), or (b) the C-backend struct-layout
   assignment (`cg.cc`, in the spirit of issue 003's fix) needs to
   merge multiple base structs' field offsets correctly.
3. Diamond inheritance and redefinition-across-bases (MRO
   resolution) are a strictly harder follow-on; this issue is
   scoped to the simplest case (disjoint fields, no shared
   ancestor) since that already fails.

## Verification plan

1. The `C(A, B)` repro above compiles and prints `1` then `2`.
2. A variant where `C` also defines its own method/field
   alongside the two inherited ones.
3. `tests/class_inheritance.py` (existing single-inheritance test)
   continues to pass unchanged.
4. Add `tests/multiple_inheritance_basic.py` + `.exec.check` — no
   multi-base-class test exists today (the existing
   `class_inheritance.py` only exercises single inheritance,
   `class_inherited_attrs.py` similarly single-chain).

## What this unblocks

Multiple inheritance (mixins in particular — combining independent
capability classes) is a common Python pattern; today it fails
with a confusing internal-type-error cascade rather than either
working or being cleanly rejected.
