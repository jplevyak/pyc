# Issue 018: Using `dict`/`set` with two different element types in one program fails to compile

**Status:** open.
**Affects:** `dict`'s shared internal comparison logic
(`__pyc__/07_dict.py`'s `_keys[i] == key` checks in `__getitem__`/
`__setitem__`/`get`/`__contains__`) and `set`'s equivalent
(`__pyc__/08_set.py`'s `_items[i] == item` in `__contains__`/`add`/
`discard`); FA's `BOXING` type-violation path
(`ifa/analysis/fa.cc:2864-2869`, `ATypeViolation_kind::BOXING`).
**Related:** discovered while writing regression tests for issue 009
(dict comprehensions) — confirmed unrelated to comprehensions or to
that fix; reproduces identically with two plain flat dict literals
and zero comprehension code involved. Also affects `set` (issue
008's new class) identically — same shared-linear-scan-comparison
shape, same failure.

## Symptom

```python
squares = {1: 1, 2: 4, 3: 9}
print(squares[3])
words = {"a": 1, "b": 2}
print(words["b"])
```

```
warning: illegal primitive argument type 'x' illegal: str
  called from __pyc__:627
__pyc__:332: expression has mixed basic types:( int64 str )
  called from __pyc__:620
...
squares[3]: ISO C++ forbids comparison between pointer and integer [-fpermissive]
  t1 = _CG_prim_equal(t2, _CG_Symbol(2816, "=="), _CG_String("a"));
```

...and the C compile fails outright. Each dict works fine
*individually* — it's specifically having an `int`-keyed dict and a
`str`-keyed dict both live in the same program that breaks. The
identical shape reproduces with `set`:

```python
nums = {1, 2, 3}
print(len(nums))
strs = {"a", "b"}
print(len(strs))
```

(same `"has mixed basic types:( int64 str )"` failure, from `set`'s
`__contains__`/`add` this time.)

## Root cause (partially traced)

`dict.__getitem__`/`__setitem__`/`get`/`__contains__` all do
`self._keys[i] == key` — one shared method body per dict operation,
not cloned per key type the way e.g. `list`'s element type
specializes per instantiation. When the program also constructs a
dict with a *different* concrete key type, FA ends up flowing both
`int64` and `str` values into the same `key`/`_keys[i]` AVar during
specialization of these shared methods, and flags a `BOXING`
violation (`ATypeViolation_kind::BOXING`, "has mixed basic types") —
`int64` and `str` don't share a representable layout without boxing,
and pyc apparently doesn't (or can't, for `dict`'s specific call
shape) box here, so it falls through to the C backend with an
unresolved/mismatched type, producing invalid generated C.

Not yet traced further — unclear whether the fix is "FA should
box these" (making it work, at some cost) or "`dict`'s methods need
per-key-type specialization" (mirroring how other generic containers
apparently already get this right, e.g. `list` holding different
element types across separate instances works fine per the existing
test suite and this session's issue 017 stress-testing).

## Verification plan

1. The repro above compiles and both `squares[3]` (9) and
   `words["b"]` (2) print correctly.
2. A single program using `dict[int, ...]`, `dict[str, ...]`, and
   `dict[float, ...]` (three key types) together.
3. Existing single-key-type dict tests (`tests/dict_basic.py`,
   `tests/dict_methods.py`, `tests/dict_comprehension_basic.py`)
   continue to pass unchanged.

## What this unblocks

Realistic programs routinely use dicts keyed by different types in
different places (e.g. a `dict[str, int]` config alongside a
`dict[int, str]` lookup table) — this is a significant, easy-to-hit
limitation for anything beyond a single-key-type toy program. The
existing dict test suite (`tests/dict_basic.py`, `tests/dict_methods.py`)
happens to only ever use string keys, which is exactly why this had
gone unnoticed.
