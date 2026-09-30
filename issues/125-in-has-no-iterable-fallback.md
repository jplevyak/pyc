# 125 — `x in y` has no iterable fallback

**Status:** open. Rewritten 2026-09-28; history in git:
`git show e3b44e2c:issues/125-in-has-no-iterable-fallback.md`. Its two
instances are fixed: `range.__contains__` (`4509936e`, arithmetic,
`tests/in_range.py`) and, for the same defect in `list(x)`, the general
`object.__pyc_tolist__` fallback (`29b6e725`).

## Symptom (re-verified 2026-09-28)

```python
class Bag:
    def __init__(self): self.v = [1, 2, 3]
    def __iter__(self): return iter(self.v)
print(2 in Bag())    # CPython: True.  pyc: error: illegal call argument type expression illegal: int64
```

`emit_in_pyda` (`python_ifa_build_if1.cc`) lowers `x in y` to
`y.__contains__(x)` only. CPython falls back to the iterable protocol (and
then to `__getitem__`). Three classes carry hand-written consuming
`__contains__` methods for exactly this reason: `__pyc_generator__`,
`__pyc_iterator__` and `__dict_iter__`.

## The fix: `object.__contains__` = CPython's fallback, verbatim

```python
def __contains__(self, item):
    for x in self:             # iter(self), then __next__ until exhausted
        if x == item: return True
    return False
```

The earlier worry was that this "consumes a self-iterator". **That is
CPython's own behaviour:** its fallback calls `iter(y)` and consumes
whatever that returns, so a user class whose `__iter__` returns `self` IS
exhausted by `in` in CPython too. The only real hazard is a builtin whose
`iter()` returns itself where CPython's does not. That is pyc's `range`
(`__iter__` returns `self`, so `r = range(10); for ...; for ...` iterates
once). That is a deviation in its own right, and it belongs on
[171](closed/171-permissive-accommodations-must-be-flagged-and-non-strict.md)'s
list: make `range.__iter__` return a fresh iterator. `range` has its own
arithmetic `__contains__`, so it never reaches the fallback anyway.

Put the method on `object`, as `object.__not__` and `__pyc_tolist__`
already are, so any class defining `__contains__` overrides it. Then
delete the three hand-written copies, which are this method.

`__getitem__` as a third step: only if a program needs it; none does.

## Verification

The repro prints `True`. `tests/in_range.py` is unchanged (including `3
in r` followed by iterating `r`). The three hand-written copies are
removed, and generators, bridged iterators and dict iterators still pass.
Corpus `check`: no verdict regressions.
