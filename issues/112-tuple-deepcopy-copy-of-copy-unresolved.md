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

### What is left: a CreationSet minted on the TERMINAL pass

Four things were tried from the current base. All are ruled out, and
together they locate the problem precisely.

**1. The receiver is not the shared base sym.** Instrumenting pass 2
shows the tuple group DOES clone (`cloned=1`). The printed name
`tuple` is just `Sym::clone()` keeping its name — a name is not an
identity, and reading one as one is what produced the earlier "shared
base sym" claim. Retract it.

**2. Excluding tuples from the pass-1 no-clone path changes nothing.**
The receiver type is unchanged and h5 fails identically. (The earlier
`_CG_any` outcome was from before `a224c063`; it does not reproduce.)

**3. The group is heterogeneous — one member has a bottom element:**

```
[prim] tuple group n=4 cloned=1  cs=962[SET]  cs=1074[bottom]
                                 cs=1116[SET]  cs=1117[SET]
```

One concrete type for the group, but FA specialised contours per CS,
so the bottom one yields a `__getitem__` clone returning a different
type. Hence two candidates, same arguments, different returns.

**4. Disambiguating at the call site does not work, because the
information genuinely conflicts.** The caller's destination type is a
THIRD type, matching neither candidate:

```
[dis] want=?/0x…5d1400 | ret0=?/0x…3dbc00  ret1=?/0x…3db000
```

A `disambiguate_by_result` that picks the unique candidate whose
return type matches the call site's destination therefore finds no
match. (Worth keeping in mind for other ambiguities; it is not this
one.)

### The actual cause

`cs=1074` is minted for a new contour on the **terminal** analysis
pass. Giving it a source does not help: with a per-SITE source memory
added (any contour of the make_seq site seeds it), it reports
`seq=1` — the source is known and `vector_elems` has built the edges —
**and its element is still bottom**, because no pass remains for those
edges to propagate along.

So this is not a make_seq problem, not a tuple problem, and not a
codegen problem. It is the long-standing convergence issue: FA can
mint a CreationSet on the pass it terminates on, and that CS can never
receive types. `CreationSet::seq_src` (issues/110) fixed the case where
a source read empty on the last pass; it cannot fix a CS that has no
last pass.

The fix belongs with the stall guard / `analyze_again` machinery in
`extend_analysis` — see ifa/issues/033, 055, 057, 066. Minimally: a
pass that mints a new CreationSet must not be the terminal pass.

## Verification

- `tests/deepcopy_tuple_copy_of_copy.py` under
  `PYC_MAKESEQ=1 PYC_TUPLE_AS_LIST=1` with a `tuple.__deepcopy__`.
- `tests/deepcopy_objects.py` must pass at the flipped defaults.
- Both suites and a corpus sweep, per issues/110's standing numbers.
