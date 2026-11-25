# 216 — `dataclasses.fields/astuple/asdict/replace`; `operator.attrgetter/methodcaller`

**Status:** closed 2026-11-25 on landing, filed and done in one change.
What is left: `Field.type`, `default`, `default_factory` and `metadata`;
`is_dataclass` and `make_dataclass`; `replace(obj, **d)`; and names computed
at run time for `attrgetter` / `methodcaller` (or `getattr`).

## Symptom

`fields`, `astuple`, `asdict` and `replace` were refused at import
("dataclasses.X is not supported", [195](195-typing-and-dataclasses.md)), and
`operator` had no `attrgetter` or `methodcaller`, because each reads an
attribute by a name held in a string.

## dataclasses

Each is a method the dataclass lowering generates on every dataclass
(python_ifa_dataclass.cc), only when the program uses it. A pre-scan of
every module finds the uses first (`uses`), since a dataclass in one module
can be converted or replaced in another:

- `fields(x)` is `x.__pyc_dc_fields__()`, a classmethod, so a class or an
  instance. It returns one tuple of `Field` per class, made in the class
  body, so repeated calls return the same objects, as CPython's do. `Field`
  has `name`, `init`, `repr`, `compare`, `hash` and `kw_only`.
- `astuple(x)` / `asdict(x)` are `(v(self.a), ...)` / `{"a": v(self.a),
  ...}`, with `v` CPython's `_astuple_inner` / `_asdict_inner`
  (`pyc_lib/_dataclasses_convert.py`). A field keeps its own type in the
  tuple. The dict literal's values must have one representable type, which
  the analysis checks as for any dict. The recursion tells a nested
  dataclass from any other value by `isinstance` against
  `dataclasses.__pyc_dataclass__`, a base the lowering adds to every root
  dataclass when the program converts one; it folds per contour. A tuple
  field is mapped element by element, in each element's own type, by
  `tuple.__pyc_map__`. That is per-arity like the other generated tuple
  methods (python_ifa_main.cc), and emitted only when the conversion module
  is loaded, since unreached overloads cost analysis time (213).
- `replace(obj, k=v, ...)` is lowered at the call to
  `obj.__pyc_dc_replace_N__(k=v, ...)`, one method per sorted set of
  keyword names in the program. It returns `Cls(a=self.a, k=k, ...)` over
  the init fields. The keywords stay keywords, so they are evaluated in
  source order. A name that is not an init field raises CPython's
  `ValueError` / `TypeError`. `**changes` has no names at compile time and
  is refused.

The conversion module is imported by the lowering, after the imports were
pre-scanned, so `prescan_added_imports` (python_ifa_build_syms.cc) parses
what a lowering adds.

## operator

`attrgetter("a", "b.c")` and `methodcaller("m", e, k=f)` with constant names
are lowered per call site to a class generated at the top of the module,
whose `__call__` is `(obj.a, obj.b.c)` / `obj.m(self.a0, k=self.k0)`, and
whose `__repr__` is CPython's. The call becomes `__pyc_attrgetter_N__()` /
`__pyc_methodcaller_N__(e, f)`, so methodcaller's arguments are evaluated
once, where it is made, as in CPython. A name that is not a constant, a
`*`/`**` argument, or the bare name used as a value is refused.

## Found on the way: a frozen dataclass with a default raised at class creation

At HEAD:

```python
@dataclass(frozen=True)
class Seg:
    pair: int = 1
```

The class body's `pair = <default>` is a member store, and `pair` has the
frozen setter `__pyc_set_pair__`, so the store raised `cannot assign to
field 'pair'` while the class was being made. The exception stayed pending
to the next check: `Unhandled exception` at the first call that could
raise, and `Seg.pair` unresolved. In the conversion test it made `astuple`
take its exception branch and return an uninitialized tuple, which crashed
`__str__`. The lowering now marks those class-body stores raw
(`is_raw_field`), as it already did `__init__`'s.

## Verification

- `tests/dataclasses_functions.py`: all four, nested dataclasses, a tuple
  field, a list field copied, a frozen class, a subclass, `__post_init__`
  through `replace`, `map(astuple, ...)`, and both `replace` errors.
- `tests/operator_attrgetter_methodcaller.py`: one and several names, a
  dotted name, `sorted`/`max`/`map` keys, evaluation once, and the reprs.
- `tests/dataclass_frozen_default.py`: the class-creation bug.
- `tests/operator_attrgetter_dynamic_refused.py`,
  `tests/dataclasses_replace_kwargs_refused.py`.

All are identical to CPython on the C and LLVM backends and under
`--strict`.

`make test`: every gate passes, 612 passed and 0 failed on both backends.
Corpus: `check__default__f2b1544d+8c6f33fe` is identical to
`check__default__5499f7f7+7e701eac` (the sweep of the tree before) in every
column, contour counts included. No corpus program uses these, and each
generated method, the marker base and `tuple.__pyc_map__` exist only in a
program that does.
