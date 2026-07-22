# 029 — copy.deepcopy of user objects is shallow (no per-class recursive copy)

**Status:** CLOSED. IMPLEMENTED 2026-07-17 (`251c4175`, fix sketch
option 1). Re-verified 2026-07-21: `tests/deepcopy_list.py` and
`tests/deepcopy_objects.py` both had **no `.exec.check` golden** —
they ran compile-only under `test_pyc.py` despite the "Verified"
claim below, so their runtime behavior had never actually been
diffed against real `python3` output in CI (the "deterministic"
claim below refers to repeat-compile output stability, a different
check, not a CPython comparison). Compiled and ran both directly:
`deepcopy_list.py` matched CPython byte-for-byte immediately.
`deepcopy_objects.py` did not — one line, `c1 = tree.__deepcopy__()`,
calls a compiler-synthesized method directly; real CPython objects
have no `__deepcopy__` unless the class defines one, so this line
can only ever run under pyc. Changed it to `c1 = copy.deepcopy(tree)`
(semantically identical for pyc, since `pyc_lib/copy.py`'s
`deepcopy` already just calls `obj.__deepcopy__()` per this file's
own note below) — now fully CPython-comparable, and matches
byte-for-byte. Added `.exec.check` for both tests; both now pass
execution-verified, on both backends, through `test_pyc.py`. Moved
to `closed/` as part of this pass (had sat in `issues/` unmoved
despite being fully implemented). Every
record class without its own `__deepcopy__` gets a compiler-
synthesized recursive one (gen_class_pyda, python_ifa_build_syms.cc):
shallow clone + per-field `member.__deepcopy__()` dispatch, fields
collected syntactically (`self.NAME = ...` stores, first-store
source order -- promote order must agree with `__init__`'s or the
struct layout goes nondeterministic) plus class-body attrs;
registered as a real method (selector-marked closure + cls->has
member + prototype install, nesting_depth = class-body fn + 1).
Support cast: `list.__deepcopy__` (element recursion),
`__pyc_None_type__.__deepcopy__` (identity),
`__pyc_any_type__.__deepcopy__` (shallow fallback for scalars/
strings/tuples); `pyc_lib/copy.py`'s deepcopy is now just
`obj.__deepcopy__()`. No memo table (v1): shared subtrees duplicate,
cycles don't terminate -- corpus need is trees.

Landing it surfaced and fixed FIVE latent compiler bugs:
[ifa/issues/046](../../ifa/issues/046-optional-none-field-inline-type-sum-assert.md)
(inline chain Type_SUM assert -> pre-checked, un-inlined),
[ifa/issues/044](../../ifa/issues/closed/044-mixed-length-tuple-list-len-miscompile.md)
(listish-tuple literal length off-by-one -> phantom elements),
cg.cc's `simple_move` dropping nil-typed moves into REAL locals
(uninitialized `return t0;` -- None-returning methods returned stack
garbage), the recursion pending-map fanning one call edge to every
recorded ES (degenerate `if (fn == &fn)` dispatch), and
`determine_layouts`' insertion-order offsets (same-class CSs with
re-ordered fields tripped "missmatched offsets" -- now canonical
name-ordered). Plus `_CG_prim_copy_any` (GC_size-based) for copies
whose static type is a same-class CS union.

Verified: tests/deepcopy_objects.py (nested objects, Optional[list-
of-self] trees, copy-of-copy, mutation isolation; deterministic,
both backends), suites 200/0 x2, unit 58/0. genetic2 itself now has
correct deepcopy SEMANTICS but its compile diverges in FA flow
(unbounded matcher allocation over the copy-chain unions) --
[ifa/issues/048](../../ifa/issues/048-deepcopy-flow-divergence-genetic2.md).

Original report follows.

**Status (original):** open. Found 2026-07-16 as genetic2's LAST blocker (the
example now compiles and runs deep into its GP simulation before
this bites — see issues/025's genetic2 dig).

## Symptom

`copy.deepcopy(obj)` where `obj` is a user-class instance falls
through `pyc_lib/copy.py`'s list branch to the shallow `copy` prim
(a single-level struct clone). Nested objects stay SHARED between
the "deep" copy and the source.

genetic2's failure chain: `Individual.copy()` does
`Individual(copy.deepcopy(self.genome))` where genome is a TreeNode
tree (`args`: list of child TreeNodes). Shallow copy shares every
subtree between parent and offspring individuals; `crossover()` then
grafts nodes across individuals, and with sharing a node eventually
becomes its own descendant — a CYCLE in the "tree" — and
`TreeNode.execute`'s recursion overflows the stack (SIGSEGV, same
node pointer in every frame). CPython deep-copies via runtime
reflection, which pyc doesn't have.

## What pyc's deepcopy handles today

`pyc_lib/copy.py`: recursive LIST deep copy (each recursion level
gets a monomorphic contour via recursive-ES splitting — issues/025
R1 item 5 resolution); scalars are identity (P_prim_copy scalar
case, both backends); everything else — including user objects —
is a shallow one-level clone.

## Fix directions

1. **Compiler-synthesized per-class `__deepcopy__`** (the principled
   one): at codegen (or FA concretize time), each Type_RECORD's
   layout is fully known — synthesize a recursive copy function per
   class: clone self, then for each pointer field invoke that
   field type's synthesized copy (lists recurse per element; a memo
   table handles cycles/diamonds, matching CPython's `memo` dict).
   This mirrors how clone/prim_period already work per-layout.
2. **User-provided `__deepcopy__` dispatch** (cheap partial):
   `deepcopy(obj)` tries `obj.__deepcopy__()` when the class defines
   one, else shallow. Unblocks nothing in the corpus by itself
   (examples don't define it) but is the standard protocol hook and
   composes with 1.

## Verification

- Micro: `Node(value, children)` tree; deepcopy; mutate the copy's
  grandchild; source untouched; no sharing (compare ids).
- genetic2 runs to completion (its "Epoch:" prints appear; no
  stack-overflow SIGSEGV in TreeNode.execute).
- tests/deepcopy_list.py keeps passing (list path unchanged).
