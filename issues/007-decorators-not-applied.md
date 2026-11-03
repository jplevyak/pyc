# 007 — decorators and descriptors: what is still refused

**Status:** open on the five items below. Rewritten 2026-11-03. What works
and how (`PY_decorated` applying decorators bottom-up, dotted decorators,
recursion reaching the decorated binding, `@property` getters through
`inject_property_accessors`, `@staticmethod` / `@classmethod` as markers,
the trivial `NAME = property(GETTER)` rewrite) is pinned by
`tests/decorator_*.py`, `property_getter.py` and
`property_in_imported_module.py`, and described in git
(`git show 23cd789:issues/007-decorators-not-applied.md`).

**The rule: apply a decorator or refuse it, never ignore it.** Dropping
one changes the program, a silent deviation in every mode.

## Open, in order

1. **Property setters and deleters** (`@x.setter`, `@x.deleter`,
   `property(get, set)`): refused with a named error
   (`tests/property_setter_refused.py`). The store side of the getter
   mechanism: an attribute STORE of a property name lowers to
   `__pyc_set_NAME__`, with a default on every class that writes the
   field, so dispatch picks the setter on the class that has one.
2. **A store to a getter-only property writes a field.** CPython raises
   `AttributeError`. Falls out of 1: the default store accessor on a class
   that defines the property as read-only raises.
3. **`NAME = property(GETTER)` with a non-trivial getter** is refused as
   an unimplemented builtin. The pre-scan can treat it as `@property` (an
   accessor that calls GETTER). The field rewrite for a trivial getter
   keeps winning where it applies, because it keeps the read a field.
4. **Class-based decorators** (`@Wrapper` with `__init__` and `__call__`):
   the class call in decorator position is a raw send and FA reports
   "expression has no type". Refused, not silent. Lower it as a
   constructor call whose result is called through `__call__`.
5. **A `@property` the pre-scan cannot see** (stacked under another
   decorator, on a non-method, or in the REPL, which has no pre-scan) is
   refused by `build_syms` with a named error. Leave it until a program
   needs it.

A decorator stored on a class after its definition (`N.d = f`, then
`@N.d`) is refused as an unresolved member. That is the class-attribute
mutation gap, [173](173-silent-deviations-found-by-the-strict-suite-check.md)
item 1, not a decorator one.

## Verification

Each item gets a fixture matching CPython on both backends;
`property_setter_refused` turns from a `.check_fail` into a passing test
when 1 lands. `sat` (a `@property` on a debug path) and voronoi2 (the
`property(GETTER)` form) keep their corpus verdicts.
