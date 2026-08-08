# 044 — `list.__add__` (`+`) mutated the left operand in place, unlike CPython

**Status:** fixed 2026-08-08, found while diagnosing
[issues/025](../025-shedskin-examples-coverage.md)'s TODO list item 2
(rubik2's degenerate BFS "solution").

**Affects:** `pyc_c_runtime.h`'s `_CG_list_add_internal` (C backend,
`static inline`) and `pyc_runtime.c`'s `_CG_list_add` (LLVM backend,
its own out-of-line copy with int64 element sizes — see that file's
top-of-file comment on why the two exist independently). Both back
`list.__add__` (`__pyc__/04_sequence.py`'s plain `list+list` path,
non-`isinstance(l, tuple)` branch).

## Symptom

Minimal repro:

```python
a = [1]
b = a + [2]
print(a)
print(b)
```
- CPython: `[1]` / `[1, 2]` — `+` never touches either operand.
- pyc (before this fix, **both** backends): `[1, 2]` / `[1, 2]` — `a`
  silently gained the concatenation as a side effect of merely
  evaluating `a + [2]`.

This is invisible whenever the left operand is never read again (the
overwhelmingly common case — `x = a + b` where `a` goes unused
afterward), which is why it survived undetected through the existing
test corpus. It becomes real, silent data corruption the moment the
left operand is aliased somewhere else — an object field, a variable
still in scope, an entry in a container:

```python
class node:
    def __init__(self, route):
        self.route = route
    def extend(self, move):
        return node(self.route + [move])

start = node([])
children = [start.extend(i) for i in range(4)]
for c in children:
    print(c.route)
```
- CPython: `[0]`, `[1]`, `[2]`, `[3]` — four independent routes.
- pyc (before fix): `[0, 1, 2, 3]` printed **four times**. Every
  `extend()` call reads `start.route` (the same list header each
  time, since `start` itself is never reassigned), and each call's
  `self.route + [move]` mutated that shared header's backing buffer
  in place before wrapping it into a new `node`. All four children's
  `.route` field end up pointing at the same, fully-accumulated
  header.

This exact shape (`self.field + [item]` inside a per-instance
constructor, called across a loop of instances sharing one receiver
list) is precisely what rubik2.py's phase-0 BFS does
(`cube_state.apply_move`: `return cube_state(newstate,
self.route+[move])`, called against the same `cur_state.route` for
many different `move`s in the inner `for move in phase_moves[phase]`
loop) — every generated child state's `.route` collapsed onto one
shared, ever-growing list, and (separately, see "Residual" below)
`state_ids`'s dedup keys derived from corrupted state also stopped
deduping correctly, producing the reported ~7675-move degenerate
"solution" where Thistlethwaite's algorithm guarantees ≤7.

## Root cause, traced via generated C

`__pyc__/04_sequence.py`'s `list.__add__` (non-tuple branch) calls the
C primitive directly on `self` as the first argument:
```python
return __pyc_c_call__(__pyc_primitive__(__pyc_symbol__("merge_in"), self, l),
                      "_CG_list_add",
                      list, self, int, l, ...)
```
and `_CG_list_add_internal` (`pyc_c_runtime.h`, pre-fix):
```c
static inline _CG_list _CG_list_add_internal(_CG_list l1, _CG_list l2, uint32 size1, uint32 size2) {
  uint32 s1 = _CG_prim_len(0, l1), s2 = _CG_prim_len(0, l2);
  uint32 size = size1 ? size1 : size2;
  _CG_list x = (_CG_list)MALLOC(size * (s1 + s2));
  if (s1) memcpy(x, _CG_list_ptr(l1), s1 * size);
  if (s2) memcpy(((char *)x) + s1 * size, _CG_list_ptr(l2), s2 * size);
  _CG_list_len(l1) = s1 + s2;        // <-- mutates l1's own header
  _CG_list_total_len(0, l1) = s1 + s2; // <-- mutates l1's own header
  _CG_list_ptr(l1) = x;               // <-- mutates l1's own header
  return l1;                          // <-- returns l1 itself, not a fresh object
}
```
It allocates a fresh *data buffer* `x` and copies both operands'
elements into it correctly — but instead of wrapping `x` in its own
fresh list header and returning that, it overwrites `l1`'s existing
header fields (`len`, `total_len`, `ptr`) to point at the new buffer
and returns `l1`. That is exactly the correct contract for an
in-place operation (`list.append()`/`extend()`/`__iadd__`, backed by
the sibling `_CG_list_resize_internal`, which correctly mutates in
place because CPython's `append()` does too) — but `__add__` must
never mutate either operand.

`pyc_runtime.c`'s `_CG_list_add` (the LLVM backend's independent,
out-of-line copy — see that file's header comment on why the C and
LLVM backends each carry their own primitive-implementation copies)
had the identical bug, confirmed separately and fixed identically.

## Why this wasn't caught by [issues/017](017-multi-instance-mutation-corruption.md)

Issue 017 (closed) diagnosed a symptom that looks superficially
similar — two `dict`/`set` instances silently sharing storage — but
root-caused it to `dict`/`set` lacking their own `__init__`
(inheriting a *shared prototype's* already-populated list fields, a
classic Python mutable-class-attribute footgun), fixed by giving both
an explicit `__init__`. That issue's writeup explicitly records
prototyping a broader "make list_resize/list_add/setslice always
allocate fresh" fix and reverting it after it broke 5 existing tests
— but every one of those 5 tests turned out to depend on
`_CG_list_resize_internal`'s in-place mutation specifically (the
correct contract for `.append()`), not on `_CG_list_add_internal`'s
(the *incorrect* contract for `+`). The two functions were bundled
into one experiment and reverted together; `_CG_list_add_internal`'s
own, `__add__`-specific bug was never independently isolated or
fixed. Verified now: this fix touches **only** `_CG_list_add_internal`
/ `_CG_list_add` (not `_CG_list_resize_internal`, `_CG_list_mult_internal`,
or `_CG_list_setslice_internal`, all of which keep their existing,
correct-for-their-callers in-place semantics), and the full suite
(both backends) is clean — confirming the two functions' contracts
really were independent all along.

## Fix

Made `_CG_list_add_internal` (`pyc_c_runtime.h`) and `_CG_list_add`
(`pyc_runtime.c`) fresh-allocate the list header too, mirroring the
pattern `_CG_list_getslice_internal`/`_CG_list_getslice` already use
correctly for `list[a:b]` just below each in the same files — neither
operand's header is touched:
```c
_CG_list x = _CG_ptr_to_list((_CG_list)MALLOC(size * (s1 + s2) + SIZEOF_LIST_HEADER));
_CG_list_len(x) = s1 + s2;
_CG_list_total_len(0, x) = s1 + s2;
_CG_list_ptr(x) = x;
if (s1) memcpy(_CG_list_ptr(x), _CG_list_ptr(l1), s1 * size);
if (s2) memcpy(((char *)_CG_list_ptr(x)) + s1 * size, _CG_list_ptr(l2), s2 * size);
return x;
```

## Verification

- Both repros above: pyc now matches CPython exactly, on **both**
  backends (C and LLVM) — with one caveat: the `route=None` default
  form of the class repro (`def __init__(self, route=None): self.route
  = route or []`) segfaults on the LLVM backend for an unrelated,
  pre-existing reason (a class constructed both with and without its
  default arg in the same program — nothing to do with list `+`; see
  [ifa/issues/088](../../ifa/issues/088-llvm-class-list-field-plus-construct-segfault.md)).
  `tests/list_add_no_mutate.py` below sidesteps this by using a
  required (non-default) constructor parameter, which is unaffected.
- New regression test `tests/list_add_no_mutate.py` (both scenarios
  above, `.exec.check` generated from `python3`).
- The mini-BFS repro from the original item-2 investigation (4-int
  state space, class-based, `self.route + [move]` per transition) now
  finds the correct 12-move solution matching CPython exactly, on the
  C backend (previously: a degenerate 2112-move cycling pattern, on
  both backends).
- Full `test_pyc.py`, both backends (`PYC_FLAGS=-b`): 257 passed / 11
  expected-fail / 0 failed / 4 skipped on each — no regressions
  relative to the pre-fix baseline (256/11/0/4; the +1 pass is the new
  test).

## Residual: rubik2.py itself still doesn't complete in reasonable time

Fixing this bug was necessary but **not sufficient** to make the
original rubik2.py motivating case finish. Isolated post-fix
(`stdbuf`-forced unbuffered output ruled out a display artifact; `ps`
showed sustained 100% CPU and `strace` showed a heap that kept
mmap'ing exponentially larger blocks, i.e. genuine unbounded
allocation, not a hang-while-idle): the full program still doesn't
print even its first line within 60s post-fix, though it no longer
produces the old *visibly degenerate* output either (pre-fix, the
same binary printed progressively worse-and-worse move sequences and
was still running when killed at 120s — so this is not simply "was
already this slow," something is qualitatively different, and it
wasn't isolated further here). Bisection ruled out `cube_state` alone,
`random` alone, and the randomize/apply_move prefix alone (all fast,
correct, and unaffected by this fix) — the hang requires the full
BFS solve loop to be present, consistent with pyc's whole-program flow
analysis giving `cube_state` different CreationSet specializations
depending on downstream call shapes. `set.__contains__`
(`__pyc__/08_set.py`) is a **linear scan** over `_items`
(`for i in range(self._len): if self._items[i] == item: ...`), not a
hash lookup — the BFS's `next_id not in state_ids` dedup check is
therefore O(n) per call against a state_ids set that itself grows
with the search frontier, making the whole search at least O(n²) in
the (currently unknown, and per the strace evidence possibly
unbounded) number of distinct states visited. This is a plausible
*contributing* factor but not confirmed as the sole or even primary
cause — not root-caused further here given the depth already spent
tracing the list.__add__ bug in this same session. Left for a
follow-up issue rather than guessed at further.

## What this unblocked

Any pyc program building an object whose field holds `x + y` (any
list `+`, not just object-field-in-a-loop) where the left operand
`x` is read again afterward was at risk of silent data corruption —
independent of classes/OOP specifically (the bare `a = [1]; b = a +
[2]` repro has no class involved at all). This is now correct on both
backends. rubik2.py's own end-to-end completion remains open (see
"Residual" above).
