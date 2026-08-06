# 032 — `x in d.keys()` / `x in d.values()` / `(k, v) in d.items()` unresolvable — `__contains__` never defined on dict view iterators

**Status: FIXED 2026-08-05.** Found while investigating
`shedskin_examples/webserver/webserver.py`, which failed to compile
cleanly (`illegal call argument type` / `expression has no type` on
`elif s in self.mapSocks.keys():`).
**Affects:** `__pyc__/07_dict.py`'s `__dict_iter__` and
`__dict_items_iter__` classes (what `dict.keys()`/`.values()`/
`.items()` return).
**Related:** [ifa/issues/closed/076](../../ifa/issues/closed/076-mutation-driven-receiver-divergence-not-cloned.md)/[078](../../ifa/issues/closed/078-class-body-default-plus-init-override-permanently-unions.md)
(the session immediately preceding this one; a different class of
`dict`/`set` bug, not the cause here despite superficially similar
symptoms — see "Root cause").

## Symptom

```python
d = {"a": 1}
if "a" in d.keys():   # or: 1 in d.values() / ("a", 1) in d.items()
    print("yes")
```

Minimal repro (module-level, no class needed — traced down from
`webserver.py`'s `self.mapSocks.keys()` through 15 intermediate
reductions):

```python
d = {}
d[1] = "one"
s = 1
if s in d.keys():
    print("in dict")
```

Fails identically to the corpus symptom:

```
warning: illegal call argument type expression illegal: 
    if s in d.keys():
                    ^
warning: expression has no type
```

`s in d` (no `.keys()`) works correctly — the gap is specific to the
*view/iterator* objects `.keys()`/`.values()`/`.items()` return, not
`dict` itself.

## Root cause

`x in y` lowers unconditionally to a direct dispatch,
`y.__contains__(x)` (`python_ifa_build_if1.cc:168`) — there is no
fallback to the general iterable protocol (repeated `__next__` +
comparison) when the right operand has no `__contains__`. `dict.keys()`
/`.values()`/`.items()` return `__dict_iter__`/`__dict_items_iter__`
instances (`__pyc__/07_dict.py`), and neither class has ever defined
`__contains__` — not a precision/imprecision bug (FA correctly
determined no such method exists), a straightforward missing-method
gap. Every use of this ordinary idiom against a `.keys()`/`.values()`/
`.items()` result was unconditionally broken, regardless of how
precisely-typed the surrounding program otherwise was.

(Initially suspected this was another instance of
[076](../../ifa/issues/closed/076-mutation-driven-receiver-divergence-not-cloned.md)'s
mechanism, since `__dict_iter__` still has bare class-body defaults
— `_keys = []` etc. — the exact shape 076 removed from `dict`/`set`
itself. Ruled out: the symptom here is a missing method, confirmed by
tracing the generated C directly — `dict::keys()` resolves fine;
`__dict_iter__::__contains__` simply doesn't exist to dispatch to.
The class-body-default shape may still be a latent 078-style
precision hazard for `__dict_iter__`/`__dict_items_iter__` — not
surveyed as part of this fix, since it's orthogonal to this bug.)

## Fix

Added `__contains__` to both classes, linear-scan style matching
`dict.__contains__`'s own implementation immediately below in the same
file (this is a snapshot iterator per the class's existing docs, not
CPython's O(1) hash-backed view — matches the class's already-stated
convention, not a new one):

```python
# __dict_iter__
def __contains__(self, key):
    i = 0
    while i < self._len:
      if self._keys[i] == key:
        return True
      i += 1
    return False

# __dict_items_iter__
def __contains__(self, item):
    i = 0
    while i < self._len:
      if self._keys[i] == item[0] and self._vals[i] == item[1]:
        return True
      i += 1
    return False
```

## Verification

- Minimal repro and the new `tests/dict_keys_values_membership.py`
  (added, covers `.keys()`/`.values()`/`.items()` membership, present
  and absent, plus an empty dict): compile clean, output matches
  `python3` byte-for-byte.
- `webserver.py`: compiles with **zero** warnings (was 5 warnings);
  actually runs end-to-end — started the compiled binary, sent it a
  real HTTP request via `curl`, got the correct response back.
- `ifa --test`: 58/58.
- `test_pyc.py`, C and LLVM backends, `PYC_CSM` unset: 240/11/0/4
  both (239 baseline + the 1 new test, 0 regressions).
- `test_pyc.py`, C and LLVM backends, `PYC_CSM=2`: 236/11/4/4 both
  (235 baseline + 1, same 4 pre-existing failures).
- `shedskin_sweep.sh`, both `PYC_CSM` settings: one clean gain in
  each (`webserver`: `COMPILED_C_WARN` → `COMPILED_C`), zero
  regressions, diffed directly against saved pre-fix `results.tsv`.

## What this unblocks

`x in d.keys()` / `x in d.values()` / `(k, v) in d.items()` are
ordinary, common Python idioms — this was a silent, total gap for all
three, not an edge case. `webserver.py` is the one corpus example that
happens to hit it, but any user program using this pattern was
equally broken before this fix.
