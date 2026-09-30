# 007 — decorators and descriptors: what is applied, what is refused

**Status:** open for the residuals below. Rewritten 2026-09-29, when
[171](closed/171-permissive-accommodations-must-be-flagged-and-non-strict.md) #12
and #13 landed. The earlier history is in git:
`git show e3b44e2c:issues/007-decorators-not-applied.md`.

**The rule for any decorator: apply it or refuse it, never ignore it.**
A decorator pyc cannot apply changes the program, so dropping it is a
silent deviation in every mode.

## What works

- `PY_decorated` applies decorators bottom-up, as
  `name = dN(...(d1(fn)))`. That covers closure-wrapping decorators, a
  decorator returning a different function, parameterised decorators, and
  stacked applications (`@double @double` prints 8).
- **Dotted decorators** (`@mod.dec`, `@Cls.static_dec`, `@obj.attr`) are
  resolved and applied: the first name through the scope stack or as a
  module, then one attribute read per segment (`tests/decorator_dotted.py`).
  They were a silent no-op. A decorator that cannot be resolved is now an
  error.
- **Recursion inside a decorated function reaches the decorated binding**,
  as in CPython. A decorated def is no longer linked in `def_internal_fn`,
  so the name reads its binding (`tests/decorator_recursion.py`: a counting
  decorator counts 5 calls for `fact(5)`, and a memoising `fib(25)` is fast).
  A NESTED decorated def is closure-converted before it is decorated. The
  decorated path used to skip that, and FA aborted (`unique_AVar`), even
  with no recursion (`tests/decorator_nested_capture.py`). Its own name is a
  carrier field written once, after decoration, so a later rebinding of the
  name in the enclosing function is refused
  (`tests/decorator_recursion_rebind_refused.py`).
- **`@property` getters** (`tests/property_getter.py`). A descriptor is
  TYPE-DIRECTED, so the choice is left to dispatch. `inject_property_accessors`
  (`python_ifa_main.cc`) renames each class's `@property def NAME` to
  `__pyc_get_NAME__`. Every builtin class gets a default `__pyc_get_NAME__`
  that reads the field, and user classes inherit it through `object`.
  `build_if1` lowers every attribute READ of NAME to that accessor call. A
  class with the property calls its getter. A class with a data field NAME
  (voronoi2's case) reads the field. A builtin whose attribute has the same
  name (`list.count`) returns its own bound method.
- `@staticmethod`/`@classmethod` are compile-time markers. The read-only
  `NAME = property(GETTER)` form with a trivial getter is rewritten to a
  field (117, `rewrite_class_properties`). Reflected ordering is on `object`.

## Open, worst first

1. **Setters and deleters** (`@x.setter`, `@x.deleter`, `property(get,
   set)`): refused with a named error (`tests/property_setter_refused.py`).
   Implementing them needs the store side of the same mechanism. An
   attribute STORE of a property name would go through a
   `__pyc_set_NAME__` accessor, with a default that writes the field.
2. **A store to a getter-only property** (`o.x = 1` where `o`'s class has
   `@property x`) writes a field. CPython raises `AttributeError`. Needs the
   store accessor from 1, whose default on a property class raises.
3. **`@property` pyc cannot see ahead of time is refused.** That means a
   property in a module reached only through an import (the pre-scan runs
   before imports are resolved), a property stacked with other decorators,
   and `@property` on a non-method. Refused by `build_syms` with a named
   error. The REPL has no pre-scan, so every `@property` there is refused.
4. **Class-based decorators** (`@Wrapper` with `__init__`/`__call__`): the
   class call in decorator position is emitted as a raw send, and FA
   reports "expression has no type". Refused, not silent.
5. **`NAME = property(GETTER)` with a non-trivial getter** is not
   rewritten by 117, and `property` is an unimplemented builtin, so it is
   refused as one. The pre-scan could treat it as `@property` (an accessor
   that calls GETTER). 117's field rewrite should still win for a trivial
   getter, because it keeps the read a field.
6. **A decorator stored on a class after its definition** (`N.d = f` then
   `@N.d`): the class has no member `d` when it is read, so it is refused
   ("unresolved member"). This is the class-attribute mutation gap
   ([173](173-silent-deviations-found-by-the-strict-suite-check.md) item 2),
   not a decorator one.

## Verification

The tests named above pass on both backends. `sat` (whose `@property
name` is on a debug path) keeps its corpus verdict. voronoi2 uses the
`NAME = property(GETTER)` form, which 117's rewrite handles, not the
decorator.
