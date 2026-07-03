# Issue 020: `str(x)` (the builtin call) doesn't work for any type

**Status:** open.
**Affects:** `__pyc__/01_str.py` (`class str` has no `__init__`/
`__new__` that accepts a value to convert); `build_builtin_call_pyda`
(`python_ifa_build_if1.cc`) has no special-case lowering for a call
to the `str` class the way it does for `print`, `super`,
`__pyc_symbol__`, etc.
**Related:** discovered twice — first while implementing issue 006
(f-strings; sidestepped by having f-string interpolation call
`.__str__()`/`.__repr__()` directly instead of going through `str(x)`
call syntax), then again while verifying issue 019's fix
(`str(some_dict)` failed, confirmed unrelated to `dict` — a plain
`str(5)` fails identically).

## Symptom

```python
x = 5
s = str(x)
print(s)
```

```
warning: 's' has no type
str_builtin2.py:2: illegal call argument type 'str' illegal: str
str_builtin2.py:2: illegal call argument type 'x' illegal: int64
str_builtin2.py:2: expression has no type
```

Compiles with exit code 0 despite the errors (same "silent success,
broken program" shape as several other issues this session); `s`
ends up with no usable value.

Calling the *method* form (`x.__str__()`) works fine — this is
specifically about `str(x)` as a builtin function/constructor call.

## Root cause (partially traced)

`class str` (`__pyc__/01_str.py`) has no `__init__`/`__new__`
accepting a value argument — it only defines methods like
`__add__`, `__str__`, `__eq__`, etc. that operate on an *already
constructed* string. `str(x)` at the Python-source level is an
ordinary call to the `str` class; since pyc's generic class-call
lowering treats this like calling any other class's constructor
(clone prototype, call `__init__` with the given args), and `str`
has no `__init__` that accepts `x`, the argument doesn't get
consumed the way it should (converting `x` to its string form).

`PYTHON_FRONTEND.md` §11 claims `__pyc_to_str__` is "invoked by the
frontend during lowering of ... `str(...)` ... f-strings" — but no
such special-case exists in `build_builtin_call_pyda`
(confirmed: searched for `"str"`/`sym_str`/`str(x)` handling in
`python_ifa_build_if1.cc`, found nothing). That comment appears to
describe intended/aspirational behavior that was never implemented,
not current behavior.

## Proposed fix sketch

Two options, similar shape to `print()`'s existing special-case in
`build_builtin_call_pyda`:

1. **Frontend special-case**: detect a call to the `str` class with
   exactly one argument in `build_builtin_call_pyda` (mirroring the
   existing `f == sym_print` branch) and lower it to
   `call_method(..., arg, sym___str__, result, 0)` — i.e. `str(x)`
   becomes `x.__str__()` directly, sidestepping the constructor-call
   path entirely.
2. **Class-level fix**: give `class str` an `__init__(self, x=None)`
   (or `__new__`) that, when called with an argument, delegates to
   `x.__str__()` and copies the result's content into `self`. More
   in keeping with "ordinary class, no frontend special-casing," but
   requires `str`'s prototype-clone construction to support
   "initialize from an arbitrary already-computed string value,"
   which may need its own investigation given `str`'s value-type
   representation.

(1) mirrors the `print()` special-case that's already there and is
the smaller change; likely the better starting point.

## Verification plan

1. `str(5)` → `"5"`, `str(3.14)` → `"3.14"`, `str("hi")` → `"hi"`,
   `str(True)` → `"True"`.
2. `str(some_custom_object)` calls the object's own `__str__`
   override if it has one (matching `x.__str__()`'s existing
   behavior).
3. `str()` with zero arguments still produces an empty string (the
   existing no-arg constructor path, unaffected by this fix).
4. Add `tests/str_builtin_call.py` + `.exec.check` — no test
   exercises `str(x)` call syntax today (existing string tests only
   use string *literals* or the `.__str__()`/`%`-formatting paths).

## What this unblocks

`str(x)` is one of the most commonly used builtin conversions in
Python (string building, formatting, debugging); right now it
silently fails for every type, forcing anyone who hits it to
discover and use the `.__str__()` method form instead.
