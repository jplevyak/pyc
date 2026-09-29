# 110 — `tuple(iterable)` returns a list (a deviation in every mode)

**Status:** open for the residuals below. **The flip LANDED 2026-09-28:**
`PYC_MAKESEQ=1` is the default, so `tuple(x)` is a real runtime-length
tuple in every mode. `PYC_MAKESEQ=0` restores the old list. History in git:
`git show e3b44e2c:issues/110-tuple-from-iterable-returns-a-list.md`.

## Symptom (fixed)

```python
row = [1, 2, 3]
print((0,) + tuple(x + 1 for x in row) + (0,))
# CPython: (0, 2, 3, 4, 0)    pyc before the flip: [0, 2, 3, 4, 0]
```

`tests/tuple_from_iterable_is_list.py` passes. `tests/builtin_type_factory.py`
matches CPython byte for byte, so its `.python.expect_fail` is gone and its
check was re-blessed; the only line that changed is `tuple([1, 2, 3])`.

## What unblocked it: tuple deepcopy ([112](closed/112-tuple-deepcopy-copy-of-copy-unresolved.md))

`tuple` had no `__deepcopy__`. The any-type fallback was a SHALLOW copy,
and that was wrong at the old default too: `deepcopy((T(),))` shared the
`T`, printing `5 5` where CPython prints `-1 5`. Under the flip, the same
fallback made `deepcopy_objects` print 77 for 5.

`inject_tuple_methods` (`python_ifa_main.cc`) now generates one. It
**constructs** the result and never copies-then-overwrites:

- on a RECORD tuple, one literal per arity, unrolled like `__str__`, so
  each element keeps its own type. `n` is a constant there, so only one
  branch is live;
- on a runtime-length tuple (list layout, `n` not constant), `make_seq`
  over the copied elements, as `tuple.__add__` does.

Two dead ends, recorded so they are not retried:

- **An index loop into a list, then `tuple(r)`**, as this issue first
  proposed. A loop index unions the field types of a heterogeneous tuple,
  the same failure issues/119 fixed for `__str__` and `__hash__`.
- **`copy` the tuple, then overwrite each field.** Flow-insensitively,
  each field of the copy is then the UNION of the original element and
  its deep copy. For a tuple element those are two record CreationSets
  with no common C type. `deepcopy(((T(), 1), 2))` emitted a `_CG_void`
  field and segfaulted. The copy primitive is also identity for a
  list-layout container, so on a runtime-length tuple the overwrite
  rewrote the ORIGINAL.

Chasing the second dead end exposed an FA bug that is fixed
independently. A tval Var created by `make_closure_var` or `fill_tvals`
was registered only in its Fun's `fa_all_Vars`. `Fun::collect_Vars`
rebuilds that list from the CFG and can drop the Var. The per-pass reset
finds Vars only through that list and `allsyms`, so a dropped Var kept its
value across passes. Measured: a bound-method closure's receiver tval in
`__main__` still held a tuple CreationSet that no creation point had
produced for three passes. That Var reached `tuple.__getitem__` with no
element type. Such Vars are now also kept in `fa_internal_vars`, which
`foreach_var` and `foreach_avar` walk. Every pass re-derives from bottom
again.

Tests: `tests/tuple_deepcopy_elements.py` (heterogeneous and nested),
`deepcopy_tuple_copy_of_copy.py`, `deepcopy_none_or_tuple_field.py`,
`deepcopy_objects.py`. All four pass on both backends.

## Residuals

1. `t[i] = x` on a tuple mutates, where CPython raises `TypeError`.
   `tuple.__setitem__` exists only for the frontend's own use. Refuse the
   user-level store, or make it raise.
2. `zip`/`map`/`filter`/`reversed` stay eager lists, and
   `itertools.product(repeat=)` yields lists. They are equivalent under
   iteration, but the observable cases (printing the result, an infinite
   iterable) belong to [171](171-permissive-accommodations-must-be-flagged-and-non-strict.md)
   #5.

## Verification

- `make test` green on both backends at the new default (2026-09-28).
- Corpus `check` sweep `check__default__5025025e+811ac4fe`, compared with
  `…+fe09ee15` (the tree before items 3 and 4): no verdict regressions.
  - `adatron` now compiles. It runs to the 120 s cap, as CPython does.
  - `sieve` and `tictactoe` differ only in their printed timing lines.
  - `tonyjpegdecoder` hit the cap under parallel load. Run alone it
    completes in 114 s with its pre-existing `<instance>` repr difference.
    It contains no None-receiver guards.
  - Run times of the programs that do contain guards are unchanged within
    ±1 s.
