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
shape, same failure. Not a literal duplicate of any single
`ifa/issues/` file, but the same underlying gap as
[ifa/issues/063](../ifa/issues/closed/063-no-type-bucket-triage.md) (diagnosis)
/ [ifa/issues/075](../ifa/issues/075-FA-element-cs-method-split-idempotent-plan.md)
(concrete build plan) — pyc's shared `list`/`dict` container methods
aren't cloned per element/key-CS, so a program with two
differently-keyed dict instances gets one merged AVar for `key`
across both; 063 states this almost verbatim ("the single
`dict.__getitem__` contour reads one element AVar that is the union
of [multiple element types]") for the object-key/NOTYPE flavor, and
this issue is the basic-scalar-key/BOXING flavor of the identical
mechanism — 075's CSM (clone container methods per element-CS)
should fix both together. Also already informally paired with
[ifa/issues/030](../ifa/issues/030-DISPATCH-polymorphic-dispatch-fat-pointers.md)
elsewhere in the tree (`ifa/issues/073`, `ifa/issues/README.md` both
say "the 018/030 heterogeneous-union boxing family") — 030's classtag
dispatch is for object/class receivers though, so it doesn't directly
cover this issue's raw `int`/`str` scalar union; it's the sibling
bucket, not the fix vehicle. Confirmed via code: `__pyc__/04_sequence.py`
(list) already opts into `__pyc_clone_constants__`/`clone_methods_per_cs`
(closed [ifa/issues/045](../ifa/issues/closed/045-receiver-cs-method-cloning.md));
`__pyc__/07_dict.py` has none — but 045's mechanism is
receiver-*identity*-based (same type, different instance, e.g.
empty vs non-empty list), not element-*type*-based, so opting dict
into it alone would not fix this issue even if done.

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

**Update:** the second option is the one already being pursued, one
level down — see the "Related" cross-references above. `ifa/issues/075`'s
CSM (element-CS container-method separation) is designed for exactly
this shape (a shared `list`/`dict` method whose receiver is a union of
same-container-type CreationSets with divergent element types) and
should resolve this issue's BOXING failure as a side effect once it
lands, without this issue needing its own separate fix.

## Scope confirmed broader (2026-08-03): a single heterogeneous `list` literal hits this too, not just cross-instance dict/set

Found while investigating an apparent `isinstance(x, list)`-vs-union
bug (`ifa/issues/025`), which turned out to be a downstream symptom of
this exact issue reached through a different, previously-untested
shape. This issue's own filing and repro are about **two separate,
internally-consistent** containers (an all-`int`-keyed dict and an
all-`str`-keyed dict) sharing a method across instances. A **single
`list` literal whose own elements already mix basic types** hits the
identical BOXING violation and crash, with no cross-instance sharing
involved at all:

```python
lst = [1, "hello"]
for v in lst:
    print(v)
```

Compiles with the same `warning: 'v' has mixed basic types:( int64
str )`, and **crashes at runtime**: `assert(!"runtime error: matching
function not found")` — worse than this issue's own original repro
(a clean compile failure), since it compiles "successfully" (with
warnings) and only fails when actually run. Confirmed independent of
`isinstance`: `print(v)` alone triggers it; `len(lst)` (which never
touches an individual element) does not. Likely the harder of the two
shapes for `075`'s CSM to resolve on its own — CSM specializes a
*shared method* per receiver element-CS, which presumes each
*instance* is internally monomorphic to begin with (matching this
issue's own original dict/set repro); a list literal whose elements
are already a mixed-basic-type union within one instance has no
consistent per-instance element-CS to specialize by in the first
place. Worth reconfirming against 075's design once that work is
picked up — flagging now so it isn't assumed covered "for free."

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
