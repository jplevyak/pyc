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

## Diagnosed: two clones, same receiver type, different RETURN types

`PYC_DBG_DISPATCH` (already in `cg.cc`) shows the real shape — it is
not "no candidate" but **two**:

```
DISPATCH FAIL in count: fns=2 rvals=3
  cand=__getitem__(sym=0x…c1400 args=3 a2=tuple/0x…703c00 rets=1: ?/0x…7ec00)
  cand=__getitem__(sym=0x…c1400 args=3 a2=tuple/0x…703c00 rets=1: ?/0x…7e000)
```

Same `Sym`, same arity, same receiver type **pointer** (`tuple`), same
index type — and **different return types**, both anonymous.

That is the inconsistency. A list-layout tuple group takes
`define_concrete_types`' pass-1 no-clone path and every member gets the
shared base `sym_tuple`, so the receiver carries no per-CreationSet
element typing. FA meanwhile specialised two contours of
`tuple::__getitem__` that genuinely return different element types.
Codegen is then asked to pick between two C functions whose parameter
lists are identical and whose return types differ, with nothing in the
receiver to discriminate on — so it cannot, and it aborts.

### Half of it is fixed

`a224c063` collapses candidates that are identical in *every* respect,
which is a real case: it fixes
`tests/deepcopy_tuple_copy_of_copy.py`, where both clones of
`tuple::__getitem__` matched down to the element type pointer. The
check is strict enough that the differing-return-type site above
correctly does NOT collapse — the strictness is load-bearing, not
incidental.

### What is left

Give a list-layout tuple group a **cloned** concrete type per element
type instead of the shared base sym, so the receiver discriminates the
way a list's does. `sym_tuple` is `Type_PRIMITIVE`, so excluding
tuples from the pass-1 no-clone path should land them in pass 2's
Type_PRIMITIVE branch, which clones exactly when the element is
populated.

Tried once, naively, and it made things worse: the receiver came out
as `_CG_any`. That was before `CreationSet::no_static_arity` and
`seq_src` existed and before `a224c063`, so it is worth redoing from
the current base — but the `_CG_any` outcome needs explaining before
trusting it.

## Verification

- `tests/deepcopy_tuple_copy_of_copy.py` under
  `PYC_MAKESEQ=1 PYC_TUPLE_AS_LIST=1` with a `tuple.__deepcopy__`.
- `tests/deepcopy_objects.py` must pass at the flipped defaults.
- Both suites and a corpus sweep, per issues/110's standing numbers.
