# 007 — `@property` is silently ignored; the remaining decorator and descriptor shapes

**Status:** open. Rewritten 2026-09-28. It now also carries
[117](closed/117-property-and-reflected-ordering.md)'s descriptor
residual. General decorator application landed 2026-07-05. The history is
in git: `git show e3b44e2c:issues/007-decorators-not-applied.md`.

## What works

`PY_decorated` applies decorators bottom-up, as
`name = dN(...(d1(fn)))`: closure-wrapping decorators, a decorator
returning a different function, parameterised decorators, and stacked
applications (`@double @double` prints 8, re-verified 2026-09-28).
`@staticmethod`/`@classmethod` are compile-time markers. The read-only
`NAME = property(GETTER)` form, where the getter is `return self.ATTR`,
is rewritten to a field (117, `rewrite_class_properties`). Reflected
ordering (`a > b` falls back to `b.__lt__(a)`) is on `object`.

## Open, worst first

1. **`@property` with a computed getter is a silent wrong answer.**

   ```python
   class C:
       def __init__(self): self._x = 3
       @property
       def x(self): return self._x * 2
   print(C().x)        # CPython: 6.  pyc: <instance>, exit 0, no warning
   ```

   The decorator takes the unhandled no-op path, and attribute access
   reads the bound method. A descriptor is TYPE-DIRECTED: `obj.NAME` is a
   call or a field read depending on the receiver's class, and
   voronoi2 has both for one name. So the general fix is shedskin's
   rewrite *in FA*: where the receiver's class defines NAME as a
   property, the read becomes a getter call. **Until that exists,
   refuse:** "`@property` with a non-trivial getter is not supported"
   is a correct diagnostic, and printing `<instance>` is not.
2. **Setters** (`@x.setter`, `property(get, set)`): not implemented. Same
   mechanism as 1.
3. **Class-based decorators** (`@Wrapper` with `__init__`/`__call__`):
   the decorator-position class call is emitted as a raw send, and FA
   reports "expression has no type". Refused, not silent.
4. **Dotted-name decorators** (`@mod.dec`): a silent no-op historically;
   today a builtin one such as `staticmethod(deco)` used as `@N.d` fails
   with "builtin 'staticmethod' is not supported". Check that a
   user-defined dotted decorator is applied or refused, never ignored.
5. **Known CPython divergence:** a recursive call inside a decorated
   function calls the UNDECORATED function (the in-body name resolves to
   the internal fn Sym). Observable (a memoising decorator would not
   memoise the recursion). It is an accommodation, so under
   [171](171-permissive-accommodations-must-be-flagged-and-non-strict.md)
   it must be fixed, or flagged and refused under strict.

**The rule for any unhandled decorator: refuse, never ignore.** A
decorator that pyc cannot apply changes the program, so dropping it is a
silent deviation in every mode.

## Verification

The `@property` repro prints `6` or is refused with a named diagnostic.
`tests/property_readonly.py`, `tests/decorator_basic.py` and
`tests/static_method.py` keep passing. voronoi2's property/field name
collision still resolves per class.
