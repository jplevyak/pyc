# 112 — copy-of-copy of a tuple leaves `self[k]` unresolved

**Status:** open — **blocks defaulting `PYC_MAKESEQ` / `PYC_TUPLE_AS_LIST` on**
**Area:** ifa flow analysis / `__pyc__` builtin library
**Reproducer:** `tests/deepcopy_tuple_copy_of_copy.py` (18 lines)

## Symptom

With the flags on and an element-recursive `tuple.__deepcopy__`,
deep-copying a tuple **twice** aborts inside `tuple::__deepcopy__`:

```
_CG_tuple _CG_f_3068_29/*tuple::__deepcopy__*/(_CG_tuple a1):
  assert(!"runtime error: matching function not found")
```

The missing statement is `self[k]` — the generated C jumps straight
from the loop index to `T::__deepcopy__(t17)` with `t17` never
assigned. One copy is fine; the second is not.

Replacing `tuple([leaf])` with `[leaf]` passes, so it is
tuple-specific: `list.__deepcopy__` has the identical recursive shape
and handles the identical cycle (`T` holds a container of `T`).

## What it is not

Ruled out by measurement:

- **Not element loss.** All four tuple CreationSets in the failing
  program report `elem=SET` at clone time — the elements resolve fine.
  This is a *dispatch* failure, not the bottom-element family of
  issues/110.
- **Not the make_seq rebuild.** Rebuilding the copy with a self-merged
  full slice (`self.__pyc_getslice__(0, n, 1)`, which shares the
  receiver's CreationSet via the `merge` primitive and so creates no
  new CS at all) fails identically.
- **Not pass-1 no-clone.** The receiver types as the bare `_CG_tuple`,
  which suggested `define_concrete_types`' no-clone path handing out
  the shared base sym. Excluding tuples from that path makes it worse:
  the receiver becomes `_CG_any`.

## Why it blocks the flip

`tuple.__deepcopy__` cannot be omitted: without it a tuple falls
through to `__pyc_any_type__.__deepcopy__`, a one-level struct clone
that shares elements, so `deepcopy_objects` silently prints 77 where
CPython prints 5. With it, the same test aborts. Post-flip the test
fails either way, and *before* the flip it passes — because
`tuple(iterable)` returns a list today and lists deep-copy correctly.

So the flip as it stands trades correct deepcopy behaviour for either
silent aliasing or a crash. That is the same "silently wrong is worse
than visibly wrong" bar that already rejected an earlier attempt in
issues/110, so the flip is held.

It also blocks the follow-up of making `tuple.__add__` return a tuple
instead of a list: that needs `make_seq` in the library, which is only
sound once the layout flag defaults on.

## Where to look next

The receiver is a union of four tuple CreationSets that are all in one
equivalence group (so they share one concrete type) and all have
resolved elements — yet `__getitem__` does not resolve against it.
Start by dumping the receiver's `AType` at that send and comparing it
to the single-copy case, which differs only in having two CSs instead
of four.

## Verification

- `tests/deepcopy_tuple_copy_of_copy.py` under
  `PYC_MAKESEQ=1 PYC_TUPLE_AS_LIST=1` with a `tuple.__deepcopy__`.
- `tests/deepcopy_objects.py` must pass at the flipped defaults.
- Both suites and a corpus sweep, per issues/110's standing numbers.
