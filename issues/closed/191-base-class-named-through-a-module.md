# 191 — a base class named through a module (`class C(m.B)`)

**Status:** closed 2026-11-19, the day it was filed. Found compiling
shedskin's own source, whose `cpp.py` declares
`class ConstVisitor(ast_utils.BaseNodeVisitor)` after `from . import
ast_utils`. A refusal, not a wrong answer.

## Symptom

```python
import helper
class C(helper.Base):    # CPython: fine.   pyc: base not found for class 'C'
    pass
```

Every spelling failed: `import m`, `import m as a`, `import pkg.m`, and a
module bound by `from . import m` inside a package. A base written as a
bare name (`from m import Base`) worked.

## Cause

`build_syms` resolved a base with `getAST(base)->sym`, which is set for a
name and for nothing else. Modules are compile-time namespaces in pyc: `m.B`
in an EXPRESSION is resolved to the member's Sym in `build_if1`'s `PY_power`
case. Nothing did that for a base.

## Fix

- `pyc_module_member(marker, attr)` (`python_ifa_build_syms.cc`) is the one
  module-member lookup. It replaces two copies of the same code in
  `python_ifa_build_if1.cc`: `PY_power`'s and the decorator path's.
- `pyda_base_sym` resolves a base that is a name, or a chain of attributes
  each read from a module (`m.B`, `pkg.m.B`). Both the class's base list
  and `pyda_class_base_syms` (issues/116's iterator bridge) use it. Any
  other expression still fails as "base not found"; a missing member fails
  as "module 'm' has no attribute 'X'".

## Tests

- `tests/class_base_module_attr.py`: `import m`, `import m as a`, a package
  chain, `from . import helper` inside `pyc_pkg/shapes.py`, a base tuple,
  `super()` through such a base, and `isinstance`. Matches CPython on both
  backends and under `--strict`.
- `tests/class_base_module_missing.py`: the missing member's message.

Writing the test found [193](193-zero-argument-super.md).
