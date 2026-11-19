# 181 — a `@staticmethod` generator does not compile

**Status:** CLOSED 2026-10-16. Found the same day while closing issues/172 #1.
It was a refusal, not a wrong answer. It covered every DECORATED generator,
not only `@staticmethod` ones.

## Symptom

```python
class C:
    @staticmethod
    def s():
        yield 3
        yield 4
for v in C.s():
    print(v)        # CPython: 3 4.   pyc: error: unresolved call '__iter__'
```

With a yield EXPRESSION in it (`x = yield 3`), it is refused earlier with
"the value of this yield expression has nowhere to come from" (issues/172).

## Root cause

A generator is compiled as a coroutine body plus a WRAPPER that creates its
`__pyc_generator__` object and binds the body to it
(`build_generator_wrapper`, `python_ifa_build_if1.cc`). The wrapper is built
for plain functions (`gen_def_wrapper`) and for methods
(`gen_method_wrapper`). A `@staticmethod` gets neither, so `C.s()` returns the
raw coroutine handle, an `int`, and no return/send cell exists
(`gen_retcell`).

## Fix

Give a static method generator the plain-function wrapper. It has no `self`
to forward, which is exactly the plain-def case. Check `@classmethod`
generators at the same time.

## Verification

`tests/staticmethod_generator.py` (`.known_issue`) prints `3 4`, also with
`x = yield` and `send()`.

## Resolution (2026-10-16)

It was wider than `@staticmethod`. Every decorated generator failed the
same way: a plain `@deco`, `@staticmethod` and `@classmethod` alike. The
decorated path (`PY_decorated`, `python_ifa_build_if1.cc`) walks the body
and calls `gen_fun_pyda` itself, and it never did either generator step that
`PY_funcdef` does:

- **the return/send cell** (`gen_retcell`), put before the body is walked,
  because `return X` and `x = yield` read it;
- **the wrapper** that creates the generator object, built after
  `gen_fun_pyda`. The decorators now apply to the wrapper, which is the
  function a CPython `def` produces. A capturing generator's wrapper is
  dispatched on its closure carrier, which stays the decorated value, as in
  `PY_funcdef`'s closure branch.

A marker method's member is installed by a setter emitted in build_syms,
before the body exists. So for a `@staticmethod`/`@classmethod` generator,
build_syms makes the wrapper there, installs it as the member and keeps it
in `gen_def_wrapper` for `PY_decorated` to fill. It carries the marker
flags, because class-qualified calls (`find_class_method_fn`) and FA's
receiver-less instance read test them on the member's function.

issues/172's two "no generator object" refusals no longer name
`@staticmethod`. Every def now gets a wrapper, so they are internal
backstops.

`tests/staticmethod_generator.py` covers a static generator (with
arguments, called through the class and through an instance, `send`, a
return value through `yield from`), a classmethod generator building
`cls(i)`, a wrapping decorator over a generator, and a capturing decorated
generator with `send`. It is byte-identical to CPython on both backends and
under `--strict`, and its `.known_issue` is removed. `make test`: 476 passed,
16 known, 0 failed on both backends. No corpus program has a decorated
generator. Sweep `check__default__82ca40fa+813b1a1e` (`-R`): every verdict unchanged.

Not fixed here, and not generator-specific: a `@classmethod` that reads a
class attribute through `cls` (`cls.base`) does not type ("expression has
no type"), with or without `yield`, and through an instance (`C().g(2)`) as
well.
