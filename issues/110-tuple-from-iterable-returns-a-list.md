# 110 — `tuple(iterable)` returns a list (a deviation in every mode)

**Status:** open. The flip is built and HELD. Rewritten 2026-09-28; it now
also carries [112](closed/112-tuple-deepcopy-copy-of-copy-unresolved.md),
the blocker. History in git:
`git show e3b44e2c:issues/110-tuple-from-iterable-returns-a-list.md`.

## Symptom

```python
row = [1, 2, 3]
print((0,) + tuple(x + 1 for x in row) + (0,))
# CPython: (0, 2, 3, 4, 0)    pyc: [0, 2, 3, 4, 0]
```

`tests/tuple_from_iterable_is_list.py` (`.known_issue`). A one-argument
`tuple(x)` is lowered to `x.__pyc_tolist__()` (`python_ifa_build_if1.cc:958`,
"established compromise"). The same compromise makes `zip`, `map`,
`filter`, `reversed` and `itertools.product(repeat=)` return lists.
`tests/builtin_type_factory.py` carries a `.python.expect_fail` for it.

**Under [171](171-permissive-accommodations-must-be-flagged-and-non-strict.md)
this is an accommodation, and it is active under `--strict`.** It is
observable: printing, hashing, `==` against a tuple, dict keys, and
`t[0] = x` (which mutates, where CPython raises). It also manufactures
`{list, tuple}` unions that have no runtime tag (issues/025's sunfish,
`tests/list_tuple_union_method.py`).

## The fix is built: `PYC_MAKESEQ=1` (with `PYC_TUPLE_AS_LIST=1`, now the default)

`P_prim_make_seq` builds a real variable-length tuple (list layout, known
element type). It is fed through the iterator protocol
(`__pyc_seq_source__`), and `CreationSet::seq_src` remembers the last
non-empty source set, because a snapshot read one pass too late had left
the element bottom. **11 of 11 probes match CPython** under the flags:
list, slice, nested, empty, string, range, set, dict, `==`/`hash`/dedup,
and `<`/`>` between dynamic tuples. At the default, two of those abort.

## The blocker (112): copy-of-copy of a tuple

With the flags on, `tuple` needs an element-recursive `__deepcopy__`.
Without it, the any-type fallback shallow-copies and `deepcopy_objects`
prints 77 for 5. With it, deep-copying a tuple TWICE leaves `self[k]`
unresolved in `tuple::__deepcopy__`: two clones share a receiver type
and differ in return type. `tests/deepcopy_tuple_copy_of_copy.py`
reproduces it. Ruled out: element loss, the make_seq rebuild, and the
pass-1 no-clone path.

**Re-test first.** The copy-of-copy / self-feeding deepcopy family was
resolved in ifa on 2026-09-28 (closed ifa/074, ifa/168 and ifa/176:
parent-first CS routing, the owner lift, the recursive gate). Rebuild
with `PYC_MAKESEQ=1` plus the element-recursive `tuple.__deepcopy__` and
run the fixture before digging further.

## To land

1. `tuple.__deepcopy__`, `PYC_MAKESEQ=1` default, and re-blessed
   `builtin_type_factory` checks, together. Delete its
   `.python.expect_fail`.
2. `tuple.__add__` returning a tuple (it needs make_seq).
3. `t[i] = x` on a tuple raises `TypeError`.
4. `zip`/`map`/`filter`/`reversed` stay eager lists. That is iteration-
   equivalent, but under 171 the observable cases (printing the result,
   an infinite iterable) must warn or be refused under strict.
   `product(repeat=)` yielding lists goes with 1.

## Verification

The fixture prints `(0, 2, 3, 4, 0)`. The 11 probes match CPython on both
backends. `deepcopy_objects` prints 5. The same-binary corpus `check`
has no verdict regressions, and sunfish's `{list, tuple}` union is gone.
