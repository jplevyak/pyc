# Issue 027: Explicit unbound base-class method call (`Base.method(self)`) fails a type check

**Status:** open.
**Affects:** frontend call-argument type checking for unbound
method calls (`ClassName.method(instance, ...)` — the pattern
commonly used instead of `super().method(...)` to call a SPECIFIC
base's implementation, especially under multiple inheritance where
each base's `__init__` must be invoked explicitly rather than
relying on MRO chaining).
**Related:** found while stress-testing
[closed/010-multiple-inheritance-unrelated-bases.md](closed/010-multiple-inheritance-unrelated-bases.md)
(NOT part of that issue's scope — this reproduces under plain
single inheritance, no multiple inheritance needed at all).

## Symptom

```python
class A:
    def __init__(self):
        self.x = 1

class C(A):
    def __init__(self):
        A.__init__(self)
        self.z = 3

c = C()
print(c.x)
print(c.z)
```

CPython prints `1` then `3`. Pyc fails to compile:

```
unbound_call.py:7: illegal call argument type 'self' illegal: C
  called from unbound_call.py:5
unbound_call.py:7: illegal call argument type expression illegal: closure
  called from unbound_call.py:5
unbound_call.py:11: illegal call argument type expression illegal:
unbound_call.py:11: expression has no type
```

(Runtime, if forced past the diagnostics: `assert(!"runtime error:
getter not resolved")`.)

Line 7 is `A.__init__(self)`. The diagnostic says the argument typed
`self` (which is `C`, since we're inside `C.__init__`) is "illegal"
for a call that presumably expects an `A`-shaped receiver — i.e.
pyc's call-argument type check for `A.__init__` wants exactly an
`A` instance and rejects a `C` instance even though `C` is a
subtype of `A` (structurally and nominally, via `inherits_add`).

## Why (hypothesis, not traced)

`A.__init__`'s formal `self` is presumably typed via
`must_implement_and_specialize(A)` (the same mechanism used
throughout class construction — see `gen_class_pyda`, and issue
003's closed writeup for how prototype/instance CSs get their
concrete types). This mechanism likely checks receiver
compatibility structurally/nominally against `A` specifically,
without consulting `C`'s `includes`/`inherits` chain to recognize
"a `C` IS-AN `A`" for the purposes of an explicit unbound call.

Ordinary bound calls (`self.f()`, `instance.method()`) go through
`P_prim_period`/classtag dispatch (issues 003, 026, `ifa/issues/030`)
and don't hit this — pyc's dispatch-by-classtag machinery already
handles a `C`-shaped receiver correctly for method calls in
general (confirmed: normal inherited-method calls work fine, e.g.
`c.f()` calling `A`'s `f` unchanged works in every test from issue
010's reevaluation). The gap is specific to the UNBOUND call form
`ClassName.method(receiver, ...)`, which routes differently (as a
plain function call against `A.__init__`'s Fun directly, receiver
passed as an ordinary positional argument, not through period
dispatch) and apparently doesn't get the same subtype leniency.

Not verified with a debugger — this is inferred from the error
message shape and general familiarity with pyc's dispatch code from
recent sessions, not confirmed by tracing. Whoever picks this up
should start by finding where `A.__init__(self)` (a `PY_power` /
attribute-then-call on a CLASS, not an instance) gets lowered in
`python_ifa_build_if1.cc`, and compare its argument-matching path
against the period-dispatch path that already works for `c.f()`.

## Reproducer

```python
class A:
    def __init__(self):
        self.x = 1

class C(A):
    def __init__(self):
        A.__init__(self)
        self.z = 3

c = C()
print(c.x)
print(c.z)
```

Expected (CPython): `1`, `3`.

## What's needed for a real fix

1. Find the frontend lowering site for `ClassName.method(...)` calls
   (as opposed to `instance.method(...)`) and confirm it's a
   distinct code path from period dispatch.
2. Make the receiver-argument type check accept any subtype of the
   named class (consult `includes`/`inherits_add`'s chain, or route
   through the same structural/classtag-aware compatibility check
   period dispatch already uses) rather than requiring an exact
   type match.
3. Verify both backends; add a regression test (the reproducer
   above, `.exec.check` = `1\n3\n`).

## What this unblocks

- The idiomatic multiple-inheritance constructor pattern (each
  base's `__init__` called explicitly and unconditionally, rather
  than relying on `super()`/MRO chaining) — arguably the MOST common
  real-world use of this call form, since `super().__init__()` alone
  doesn't reliably reach every base in a non-linear multi-inheritance
  hierarchy without careful cooperative-`super()` design.
- Any code calling a specific base's method by name to bypass an
  overriding subclass's own version (a standard, deliberate Python
  idiom, not just a workaround).
