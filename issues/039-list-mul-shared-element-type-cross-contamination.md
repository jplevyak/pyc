# 039 — `list.__mul__`'s shared representation lets one heterogeneous list's element type leak into an unrelated, genuinely-homogeneous list

**Status: open, root-caused, not fixed.** Found investigating
`shedskin_examples/bh/bh.py` (Barnes-Hut N-body simulation).
**Affects:** `__pyc__/04_sequence.py`'s `list.__mul__`/`__rmul__`
(the mechanism), or more precisely whatever CreationSet/list-element-
type-sharing FA machinery `__mul__`'s `__pyc_c_call__("_CG_list_mult",
...)` and its `__pyc_primitive__("merge", self, self)` operand route
through — not traced past the `__pyc__`-level symptom into `fa.cc`
itself.
**Related:** [035](closed/035-list-element-cast-salvage-guard-and-set-item-union.md)/[036](closed/036-list-pop-insert-tuple-hash-and-unary-literal-defaults.md)
— the exact same architectural gap ("pyc's list-element-type inference
for the general dynamic-list representation isn't scoped per
allocation site"), first found via `tictactoe.py`'s `scores`/`set`
`_items` cross-contamination, documented there as "not fully traced to
a single call site." **This issue traces it further**: `bh.py` gives a
much cleaner, more precise trigger, and pins the mechanism specifically
to `list.__mul__`/`__rmul__` (an `n * [x]`-shaped construction) rather
than list construction/mutation in general. [018](018-dict-mixed-key-types-boxing-failure.md)/[ifa/030](../ifa/issues/030-DISPATCH-polymorphic-dispatch-fat-pointers.md)
— the general heterogeneous-container-representation gap this is a
member of.

## Symptom

`bh.py` compiles with two spurious warnings and then segfaults at
runtime (the segfault is [ifa/079](../ifa/issues/079-DISPATCH-single-candidate-dispatch-unchecked-cast.md),
a related but independently-filed issue — this issue is about the
warnings' root cause, which also feeds that one):

```
bh.py:555: warning: illegal call argument type 'b' illegal: Cell
                b.hack_gravity(self.rsize, self.root)
bh.py:565: warning: illegal call argument type 'q' illegal: Cell
                    q.expand_box(self, nstep)
```

Both `b` and `q` are elements of `self.bodies` (`Tree.bodies`,
`bh.py:479`/`499`), a list the program only ever stores `Body`
instances into. `hack_gravity`/`expand_box` are methods defined *only*
on `Body` (`bh.py:365`, `258`) — not on `Node` (the common base) and
not on `Cell` (a sibling subclass) — so pyc's diagnostic is accurate
*given* its own inferred type for `self.bodies`'s elements: that type
genuinely includes `Cell`, which has no such method.

## Root cause

`Cell` (`bh.py:390`) is the tree's internal-node class; its own field
`subp` (`bh.py:399`, `Cell.__init__`) is a **genuinely, correctly**
heterogeneous list — each of its `Cell.NSUB` slots holds either a
`Body` (a leaf) or another `Cell` (an internal node), exactly the
shape a Barnes-Hut tree needs:

```python
class Cell(Node):
    NSUB = 8
    def __init__(self):
        Node.__init__(self)
        self.subp = [None] * Cell.NSUB   # later holds Body | Cell | None

class Tree:
    def create_test_data(self, nbody):
        self.bodies = [None] * nbody     # only ever holds Body | None
        for i in range(nbody):
            self.bodies[i] = Body()
```

Both lists are constructed the same way: an `int * [None]`
list-repeat, i.e. `list.__mul__`/`__rmul__`
(`__pyc__/04_sequence.py`). **Confirmed by direct source edit**
(`shedskin_examples/bh/bh.py`, tested in a scratch copy, not
committed): replacing `Cell.subp`'s construction —

```python
self.subp = [None, None, None, None, None, None, None, None]
```

— with an explicit list literal instead of `[None] * Cell.NSUB`
**eliminates both warnings**, with nothing else in the file changed.
`Cell.subp`'s own later heterogeneity (`Body | Cell` assignments in
`load_tree`, `bh.py:324`/`332`/`417`) is exactly as expected either
way — only `Tree.bodies`'s *unrelated* element type changes, from
`Body | Cell` back down to just `Body`. This pins the leak precisely
to the shared `int.__mul__`/`list.__rmul__` construction path itself
(not, e.g., general list-element inference, or something about `Cell`/
`Body`/`Node`'s class hierarchy) — the same class of finding
[035](closed/035-list-element-cast-salvage-guard-and-set-item-union.md)
described as "not fully traced" for `tictactoe.py`'s structurally
identical `scores`/`set._items` symptom, now traced one level further.

Not traced past this point into `fa.cc`/`ifa/if1` internals (would
need the same kind of FA-level instrumentation
[ifa/071](../ifa/issues/071-FA-chess-accumulated-union-notype-cascade.md)'s
chess.py dig used) — the leading hypothesis, unconfirmed: `list.__mul__`'s
`__pyc_primitive__(__pyc_symbol__("merge"), self, self)` operand (the
first argument to its `__pyc_c_call__`) causes FA to treat the result
list's CreationSet as shared/mergeable across call sites whose source
operand resolves to a structurally-identical literal (`[None]`, a
single-element list containing the same constant, appearing at both
`Cell.__init__` and `Tree.create_test_data`) — i.e. the SOURCE
literal `[None]` may itself be a shared/interned CreationSet program-
wide, and `__mul__`'s result inherits that sharing rather than getting
its own per-call-site CS the way e.g. `__list_iter__`
(`clone_methods_per_cs`, [ifa/045](../ifa/issues/closed/045-receiver-cs-method-cloning.md))
already does for shared iterator classes. Not verified.

## Minimal reduction attempted, did not isolate further

Tried building a from-scratch minimal repro (two sibling classes of a
common base, one with a field holding a genuine `A | B` union built
via `[None] * N`, an unrelated class with a field meant to hold only
`A`, also built via `[None] * M`) — it did **not** reproduce the
warning, even though structurally it looks like the same shape as
`bh.py`'s `Cell.subp`/`Tree.bodies`. Something about `bh.py`'s fuller
context (the `load_tree`/`expand_box` methods' own recursive,
tree-mutating structure, or `Tree.root`'s own `Body | Cell` union
interacting with the two lists some other way) is load-bearing and
wasn't captured in the from-scratch attempt. A **source-reduction of
`bh.py` itself** (strip `Random`'s CLI-argument-parsing `BH` class,
replace with a minimal driver; keep `Node`/`Body`/`Cell`/`Tree`/`Vec3`/
`HG`/`Random` untouched) got to 638 lines and **does** reproduce both
warnings exactly — smaller than the 722-line original, but not a true
minimal repro. Whoever picks this up next should start from that
reduction (regenerate via the same approach: strip `BH`'s CLI parsing
and driver, replace with a bare `Tree(); create_test_data(n);
step_system(i)` loop) rather than the from-scratch attempt above.

## What this unblocks

- Would unblock `bh.py` (a corpus benchmark) from at least reaching
  runtime cleanly typed — the segfault itself is a separate, additional
  gap ([ifa/079](../ifa/issues/079-DISPATCH-single-candidate-dispatch-unchecked-cast.md)),
  so fixing this alone doesn't make `bh.py` run, but removes a
  confusing, misleading warning and the CreationSet-sharing bug this
  issue is actually about.
- General: any program with two `[None] * N`-constructed lists where
  one is genuinely heterogeneous (a real, correct union) and the other
  is meant to stay homogeneous risks the same cross-contamination —
  not `bh.py`-specific, just first isolated precisely there. Likely
  the same root cause behind `tictactoe.py`'s still-open runtime crash
  ([035](closed/035-list-element-cast-salvage-guard-and-set-item-union.md)),
  now with a much cleaner reproduction path to work from.
