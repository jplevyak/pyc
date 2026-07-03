# Issue 022: Zero-argument builtin-type constructor calls fail for every type

**Status:** open.
**Affects:** whatever code path lowers a direct-name call to a
builtin type with zero arguments — `int()`, `float()`, `bool()`,
`list()`, `str()` all fail identically; likely the same generic
constructor-call lowering that issue 020 investigated for the
1-argument `str(x)` case, but for the 0-argument arity instead.
**Related:** discovered while verifying
[020-str-builtin-call-broken.md](020-str-builtin-call-broken.md)'s
fix — that issue's own verification plan assumed `str()` (zero
args) "already produces an empty string correctly"; testing showed
that assumption was wrong, and the same failure reproduces for
every other builtin value/container type tested, so this is
distinct, broader, and pre-existing (not introduced or fixed by
issue 020's change).

## Symptom

```python
print(int())
print(float())
print(bool())
print(list())
print(str())
```

Every line fails identically at compile time:

```
_022.py:1: illegal call argument type 'int' illegal: int64
_022.py:1: illegal call argument type expression illegal:
_022.py:1: expression has no type
_022.py:1: expression has no type
_022.py:1: expression has no type
```

...and compiles with exit code 0 despite the errors (same
"silent success, broken program" shape as issue 020 and others).
At runtime:

```
_022.py.c:42: void* _CG_f_105_0(): Assertion `!"runtime error: matching function not found"' failed.
```

Expected (CPython): `0`, `0.0`, `False`, `[]`, `` (empty string),
respectively.

## Root cause (not yet traced)

Not investigated in depth. The error shape ("illegal call argument
type '&lt;classname&gt;' illegal: &lt;classname&gt;") suggests the class
itself (or some placeholder standing in for "no arguments") is
being passed as an argument to its own constructor call, which then
fails a self-referential type check — plausibly the same
`__new__`-wrapper-takes-`__init__`'s-signature machinery mentioned
in issue 020's root cause, but for the *zero-arg* arity rather than
the 1-arg arity. Given it reproduces identically across five
unrelated builtin types (three value types with hand-written
`__init__`-less classes, one container with its own `__init__`,
and `str` which also has no `__init__`), this looks like a single
shared code path in the class-instantiation lowering, not a
per-class issue.

## Verification plan

1. `print(int())` → `0`, `print(float())` → `0.0`,
   `print(bool())` → `False`, `print(list())` → `[]`,
   `print(str())` → `` (empty line).
2. Existing tests that construct these types with arguments
   (`tests/arithmetic_ops.py`, `tests/dict_basic.py`, etc.) continue
   to pass unchanged — this should be additive, not a behavior
   change for the non-zero-arg constructor path.
3. Add `tests/builtin_zero_arg_ctor.py` + `.exec.check` once the fix
   lands.

## What this unblocks

Zero-argument builtin constructor calls are a common idiom for
"give me a default/empty value of this type" (`total = int()`,
`items = list()`, initializing accumulator variables, etc.) —
right now this silently compiles to a broken program for every
builtin type, with no diagnostic pointing at the actual cause.
