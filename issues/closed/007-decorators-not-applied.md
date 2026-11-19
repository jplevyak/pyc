# 007 — decorators and descriptors

> **CLOSED 2026-11-13.** All five open items landed, in the frontend only
> (no ifa change). What is still refused is listed under Residuals; each is
> a named compile error, none is silent.

**The rule: apply a decorator or refuse it, never ignore it.** Dropping
one changes the program, a silent deviation in every mode.

The earlier history (`PY_decorated` applying decorators bottom-up, dotted
decorators, recursion reaching the decorated binding, `@property` getters,
`@staticmethod` / `@classmethod` as markers) is in git:
`git show 23cd789:issues/007-decorators-not-applied.md`.

## How a property works

A descriptor is type-directed: `o.NAME` runs the getter when o's class
defines the property and reads a field otherwise. So the choice is left to
method dispatch, for the read, the store and the delete alike
(`scan_class_properties`, `python_ifa_main.cc`):

- a class's property NAME becomes three accessor methods on it,
  `__pyc_get_NAME__`, `__pyc_set_NAME__`, `__pyc_del_NAME__`: the user's
  function renamed, a call of the function `property(...)` names, or a
  raise of CPython's `AttributeError` where the property has none;
- every builtin class gets default accessors that read, store and ignore
  the field, which user classes inherit through `object`;
- `build_if1` lowers every attribute read, store (`emit_member_store`,
  augmented stores included) and delete (`emit_del_target`) of NAME to the
  accessor call.

The store-side dispatch already existed for ctypes Structure fields
(issues/188); this reuses it.

## What landed

1. **Setters and deleters**: `@x.setter`, `@x.deleter`, `@Base.x.setter`
   in a subclass, and `property(get, set, del)`.
   `tests/property_setter.py` (was `property_setter_refused`).
2. **A store to, or delete of, a property without that accessor raises**
   `AttributeError` with CPython 3.12's message. This holds for the
   field-rename case below too: the class's own stores are marked
   (`PyDAST::is_raw_field`) and bypass the raising setter.
   `tests/property_readonly_store.py`.
3. **`NAME = property(...)` with any getter**, positional or keyword
   arguments, naming methods of the class or module-level functions. The
   accessor calls `Class.GETTER(self)`, not `self.GETTER()`: the property
   holds the function, so a subclass override does not change it.
   `tests/property_function_form.py`. The field rename for a getter that is
   only `return self._field` (issues/117) moved from `build_syms` into this
   scan and is now taken only when nothing outside the class body names
   `_field`; a subclass or outside store to it used to write a different
   field without a diagnostic.
4. **Class-based decorators** already worked when this was picked up
   (`@W`, `@W(args)`, stacked with function decorators, two instances over
   different types). Pinned by `tests/decorator_class_based.py`.
5. **`@property` the pre-scan could not see:**
   - *stacked over another decorator* (`@property` / `@deco` / `def x`):
     works, through decorated methods, below;
   - *in the REPL*: works. The REPL builds the builtin module before any
     input exists, so an input with a property is compiled by the
     whole-program path from a fresh builtin module
     (`pyc_needs_whole_program_frontend`, after `ifa_reset()`);
   - *under another decorator, or not on a method*: still refused, see
     Residuals.

**Found on the way: a decorator on a method was dropped without a
diagnostic.** `@deco def m(self)` computed `deco(m)`, bound it to a
class-body variable nothing reads, and left dispatch finding the
undecorated function. Now `hoist_decorated_method` moves the decorated def
out in front of a module-level class under a private name and makes the
method a call of the decorated value (`tests/decorator_method.py`); what
it cannot move is refused (`tests/decorator_method_star_refused.py`).

## Residuals: refused with a named error

- `@x.getter`; a `@x.setter` on a function not named `x` (CPython makes a
  second property).
- A `@property` under another decorator, or on a non-method: either hands
  code a property OBJECT, which has no value here
  (`tests/property_under_decorator_refused.py`).
- A decorated method that takes `*args` or keyword-only parameters, uses
  `super()` or a `__private` name, or is in a class that is not at module
  level.
- A decorated method called with fewer arguments than the original
  declares. The signature a caller sees in CPython is the wrapper's, which
  is not known at the call, so the method takes every parameter of the
  original and none of its defaults.
- `property(...)` with an argument that is not a plain name (a lambda).
- A decorator stored on a class after its definition (`N.d = f`, then
  `@N.d`): the class-attribute mutation gap,
  [173](../173-silent-deviations-found-by-the-strict-suite-check.md) item 1.

Two deviations to know about. The decorators of a decorated method run
just before the class statement, not inside it. And `del o.attr` on a
plain field is still a no-op (a later read does not raise), as before.

## Verification

`make test` green on both backends, 2026-11-13. The seven property and
decorator fixtures compile under `--strict`. Corpus, C backend:
`voronoi2_check` is identical to CPython; `sat` is identical apart from
its `TIME` line. voronoi2's run time is unchanged: 25 interleaved runs each of the
binary built with and without the store dispatch give the same median
(1.30 s), and the two binaries disassemble to the same instructions. The
default setters are inlined away; the generated C differs only in eight
zero-size accessor slots per class and a few dead temporaries. (A first
reading of "4 % slower" came from three runs of each binary taken one
after the other, not interleaved; it was noise.)
