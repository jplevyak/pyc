# Issue 022: Zero-argument builtin-type constructor calls fail for every type

**Status:** fixed. See "What landed" below.
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

## Root cause (confirmed)

Turned out to have (at least) two distinct causes, not one shared
code path as originally guessed:

1. **`int`/`float`/`bool`/`list`/`tuple` are never `Type_RECORD`.**
   `gen_class_pyda`'s constructor-call synthesis
   (`python_ifa_build_syms.cc`, the `__new__`-wrapper-from-`__init__`
   machinery) is gated behind `is_record = cls->type_kind ==
   Type_RECORD`. None of these five qualify: `int`/`float` are
   `Type_ALIAS` (aliased to `int64`/`float64` in
   `python_ifa_sym.cc`), `bool`/`list`/`tuple` are ifa-core builtin
   primitive types (`new_builtin_primitive_type`,
   `ifa/if1/ast.cc`). So none of them ever get a `__new__` candidate
   to dispatch a zero-arg (or any-arg) direct call to — the *only*
   reason `int(5)`/`float(5)` compiled at all before this fix is a
   separate `__coerce__`-based 1-arg conversion path
   (`cls->num_kind != IF1_NUM_KIND_NONE`, same file), which has
   nothing to consume when called with zero args. Confirmed
   `dict()`/`set()` (real `Type_RECORD` classes with an explicit
   `__init__`, issue 017) already worked fine — this is specific to
   non-record builtin value/container types.
2. **`str` had no constructor path at all** (see issue 020) — the
   1-arg case got fixed there; the 0-arg case is the same
   `Type_RECORD` gap as above (str is a primitive value type too).

## What landed

Special-cased zero-positional-arg calls to `int`, `float`, `bool`,
`list`, `tuple`, and `str` directly in `build_builtin_call_pyda`
(`python_ifa_build_if1.cc`), synthesizing each type's zero value
rather than going through class instantiation at all:
- `int()`/`float()` → an `if1_const` integer/float literal `0`/`0.0`
  (same mechanism `make_num_pyda` uses for numeric literals).
- `bool()` → `sym_false` directly (the same singleton `True`/`False`
  literals already resolve to).
- `list()`/`tuple()` → the exact `sym_primitive`/`sym_make` codegen
  an empty `[]`/`()` literal already produces (deliberately *not*
  `sym_empty_list`/`sym_empty_tuple`, ifa-core's shared singleton
  unique objects for these — returning those directly would alias
  every `list()`/`tuple()` call site to the same mutable object, the
  exact footgun issue 017 fixed for `dict`/`set`; verified no
  aliasing with a two-instance mutation test).
- `str()` → `make_string("")`.

`int`/`float` needed a by-name scope lookup (`make_PycSymbol`, same
as `str`) rather than a direct comparison against the fixed
`sym_int`/`sym_float` globals — `atom_ast->sym` for these two
resolves to the class Sym created when `__pyc__/02_numeric.py`'s
`class int:`/`class float:` are processed, not the pre-declared
ifa-core global. `bool`/`list`/`tuple` matched their ifa-core
globals directly, no lookup needed.

Along the way, found and fixed `tests/builtin_zero_arg_ctor.py`'s
would-be use of `tuple()`'s print/str path crashing the C backend —
traced to `tuple` having no `__str__` at all (falls back to the
generic `<instance>` placeholder) and no `__eq__` either, both
pre-existing and unrelated to this fix (reproduces identically for
non-empty tuple literals). Filed separately as
[023-tuple-missing-eq-str.md](023-tuple-missing-eq-str.md) rather
than fixed here — an attempted `tuple.__str__` fix introduced a new
C-backend compiler crash on a specific multi-call-site pattern, so
it needs its own investigation.

## Verification plan

1. `print(int())` → `0`, `print(float())` → `0.0`,
   `print(bool())` → `False`, `print(str())` → `` (empty line),
   `len(list())` → `0`, `len(tuple())` → `0`. ✓ (verified against
   CPython on both backends)
2. Existing tests that construct these types with arguments
   (`tests/arithmetic_ops.py`, `tests/dict_basic.py`, etc.) continue
   to pass unchanged. ✓
3. `list()`/`tuple()` produce a genuinely fresh value each call, not
   a shared aliased singleton (`a = list(); b = list(); a.append(1)`
   leaves `b` empty). ✓
4. Added `tests/builtin_zero_arg_ctor.py` + `.exec.check`.

Full `./test_pyc` + `PYC_FLAGS="-b" ./test_pyc`: 116/0 both
backends, no regressions.

## What this unblocks

Zero-argument builtin constructor calls are a common idiom for
"give me a default/empty value of this type" (`total = int()`,
`items = list()`, initializing accumulator variables, etc.) — this
no longer silently compiles to a broken program for any builtin
type.
