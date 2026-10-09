# Generic abstractions in pyc

What it would take for pyc to recognize, or be told about, code that is
generic in a type: `list.__getitem__` returns *the list's element type*,
`len` returns `int` *whatever it is given*, and a list iterator yields
*its list's* elements. What that could look like within the project's
rules, and what it would and would not do for `othello3`, measured where
possible.

**Short answer.** Generics are worth having in pyc's analysis as
*parametric transfer functions*. A builtin whose result is a projection of
its argument would act, at each call site, the way a primitive already
acts, so the union a shared builtin contour holds would stop flowing back
to every caller. That is not a split and not provenance. It can be
**recognized** from IF1, so `__pyc__` needs no annotations for the common
cases. **For othello3 it is not the main lever.** Its most expensive pass
merges on the DATA side (every list in the program shares one of two
CreationSets), and no function-side mechanism touches that. What generics
can remove is the four passes the list iterators spend catching up with
their lists, worth roughly 20% of FA time. That does not bring othello3
under the 400 s sweep cap.

## Three different things called "generic"

| | meaning | pyc today |
| --- | --- | --- |
| **parametric function** | the result type is a function of the argument types: `∀T. list[T] → T` | only primitives. `index_object`, `len`, `merge` and the other primitive transfer functions read the actual's own CreationSets, so they are parametric by construction. A Python-level `__pyc__` method is an ordinary function with one EntrySet until a demand splits it. |
| **parametric data** | a container whose field types are parameters: `list[T]`, `__list_iter__[T]` | CreationSets start one per sym (`PYC_CSDCPA1=2`; tuples and arity excepted) and split on demand ([129](ifa/issues/129-plan-demand-driven-creation-set-splitting.md)). There is no notion of a type parameter. |
| **generic emission** | one C body for many instantiations (erasure) or one per instantiation (templates) | one C function per clone. Some erasure exists already: `_CG_list` is one runtime layout whatever the element type, and a classtag dispatch over classes that agree on layout up to the slot is now one slot call (ifa/186). |

The first two are analysis questions and the subject of this document. The
third only affects code size.

## What shedskin does

shedskin's builtin models are unannotated Python of the same shape as
`__pyc__` (`lib/builtin.py`):

```python
class list(pyseq):
    def __getitem__(self, i):
        return self.unit
class __iter(pyiter):
    def __init__(self, a):
        self.unit = a
    def __next__(self):
        return self.unit
```

The genericity is in the machinery, not the source. Eager CPA makes a
template per tuple of argument types at every call, allocation sites get a
contour per template, and codegen emits C++ templates (`list<T>`). That
is splitting on a FACT ("this formal's type is a union"). pyc retired the
same thing as `PYC_CPA` ([146](ifa/issues/146-remove-all-arbitrary-splitting.md)),
and AGENTS.md's "Provenance is never the answer" explains why. So
shedskin offers pyc no mechanism to copy here, only the observation that
the type models themselves need no annotations.

## Where othello3's union lives (measured 2026-10-09)

Two runs of `57b5cb9b` with `PYC_DBG_CONTOURS=__next__` and
`PYC_DBG_CONTOURS=__len__`. The counts are distinct list CreationSets on
`list.__len__`'s receiver and distinct `__list_iter__` CreationSets on
`__next__`'s receiver, per FA pass:

| pass | list CSs | list-iterator CSs | EntrySets |
| --- | --- | --- | --- |
| 1 | 2 | 1 | 3,819 |
| 2 | 2 | 1 | 4,797 |
| 3 | 3 | 2 | 4,886 |
| 4 | 5 | 2 | 4,894 |
| 5 | 6 | 2 | 4,891 |
| 6 | 9 | 4 | 4,901 |
| 7 | 9 | 6 | 4,913 |
| 8 | 10 | 7 | 4,892 |
| 9 | 9 | 7 | 4,871 |
| 10–14 | 9 | 9 | 4,880–4,889 |

In pass 1, `__next__`'s one list-iterator contour returns
`int|str|None|float|tuple…|Line|put_*…|flip_*…`, the ~2,000-member union
of [186](ifa/issues/186-FA-pass-one-confluence-through-shared-builtins.md).
`len`'s `x` reaches `Line` and `put_*` receivers for the same reason.

Three things follow:

1. **Pass 1's merge is data-side.** Every list in the program
   (`flip_table`, `move_table`, `flippers_x`, `human_moves`, `digits`, …)
   is in one of two CreationSets, so their element channels are one
   channel. A perfectly parametric `__next__` or `__getitem__` would return
   `elem(list#9469)`, which is the whole union. That is the
   minimum-contour starting point working as designed, and its cost is
   pass 1's 184 s.
2. **The lists separate by demand in five passes** (2 → 9 by pass 6), one
   or two per pass.
3. **The iterators trail the lists by up to four passes.** Lists reach 9 at
   pass 6, iterators at pass 10. During that lag a precisely typed list
   still feeds its loop through an iterator contour shared with other
   lists, so the loop variable keeps a union and the next pass has
   something left to separate. This is the function-side confluence, and it
   is the part generics address.

The 830-class `Flip` union at the `.go()` sites is a separate matter. It is
real polymorphism under a common ancestor (`flip_table` holds every flip
class), so it is legitimate and must be hoisted, not split. Generics do not
change it.

## The design

### G1. Projection summaries: a parametric function behaves like a primitive at its call sites

Call a function **result-parametric** when everything that reaches its
return is either:

- **type-independent**: `len` returns `int` and `__eq__` returns `bool`
  whatever the receiver; or
- **a projection of a formal**: a path of element reads (`index_object`),
  field reads, and calls to other result-parametric functions, starting at
  a formal. `list.__getitem__` is `elem(self)`, `__list_iter__.__next__`
  is `elem(field(self, thelist))`, and `iter(x)` is `x.__iter__()`.

At a call edge into such a function, the call site's result is computed
from the call site's OWN actuals, by applying the path to the actual's
CreationSets, plus the type-independent part. It is not the EntrySet's
return. The EntrySet is still analyzed in full: its formals are still the
union of its callers, its return is still checked, and any violation in it
is still a demand.

What this buys is exactly the rule in AGENTS.md's "Find the confluence"
section: *the demand is observed where the union is USED, which is almost
never where the merge happened.* With G1, a union formed inside a shared
builtin contour stays there. It is observed (as an irrepresentable return
or formal) at the contour that formed it, which is the confluence, rather
than at hundreds of loops downstream that have nothing to partition.

How it fits the rules:

- **Not a split.** No contour is created or keyed differently.
- **Not provenance.** The result is a function of the actual's deduced
  types (its CreationSets), not of which call site it is. Two call sites
  passing the same CreationSets get the same answer.
- **Not `PYC_WALKCTX`** (matched call/return, on 133's do-not-retry list).
  That was a BACKWARD may-reach walk for route 4's partition, where the
  shared contour genuinely holds every caller's container, so a matched
  walk under-approximates. G1 is a FORWARD transfer function, and its
  answer is what an inlined primitive would compute: a sound
  over-approximation of what the call can return, because the value
  returned at run time is an element of the actual object, whose
  CreationSet is in the actual's type.
- **Demand stays at quiescence.** Nothing new is asked on a transient
  event.

**Recognition, not annotation.** The path is visible in IF1, built from a
formal through `index_object`, getters and returns. The one non-local
part is a call inside the body (`self.thelist.__getitem__(…)`, `x.__len__()`),
whose targets are known only from the call graph. So recognition is a
fixed point over FA's own results: a call is transparent when every
candidate it resolves to is. That uses `Fun::calls` and the resolved
candidate sets, never names.

**Codegen consequence.** The call site's lval can now be narrower than the
callee's C return type. If the callee's return is irrepresentable, that is
a violation in the callee and demand splits it, so nothing reaches
codegen. If it is representable but wider (a `void*` over two record
families), the call site needs a narrowing cast. That cast is one more
blind cast, so it must record a layout obligation like every other
(`cg_note_blind_cast`).

**What to watch.** G1 makes downstream unions smaller, and some demand that
fires downstream today may be the only thing splitting a builtin contour.
Under G1 that demand has to be raised at the contour itself. The
re-measurement in [157](ifa/issues/157-FA-all-demand-must-be-evaluated-at-quiescence.md)
and [152](ifa/issues/closed/152-FA-backtrack-the-demand-to-the-merged-creation-set.md)'s
backtracking are the relevant prior art. A program whose contour count
RISES under G1 is the signature of a demand that lost its observation
point.

### G2. Parametric data: views and builtin-internal containers

G1 does not reach the iterator. `__next__` is `elem(field(self, thelist))`,
and while one `__list_iter__` CreationSet wraps several lists, `thelist`
holds all of them, so the projection returns their combined elements.
The same holds for a container allocated inside a shared builtin, such as
`str.join`'s `parts = []` (`__pyc__/01_str.py:97`). One creation point in
one contour serves every caller.

Two shapes, and the rule each one touches:

- **A view whose identity follows its base.** `__list_iter__[T]` is one
  CreationSet per CreationSet of its `thelist` field. This is keyed on
  CreationSet partitioning, which AGENTS.md permits as identity. But it
  makes one contour split because ANOTHER contour split, which AGENTS.md
  forbids ("a contour is NEVER split because a surrounding contour was
  split"). The base is a field, not a surrounding contour, so the letter
  of the rule may not apply, but the spirit does: it is the
  `PYC_CSSPLIT`-style follow that was removed. **This needs the author's
  ruling, and it is the one piece of this design that might be a
  violation.** It also already has a hand-written form: the
  `__pyc_clone_constants__` annotation on `__list_iter__.__init__` is one of
  the nine [134](ifa/issues/134-remove-the-frontend-forced-split-opt-in.md)
  keeps, and 134 says it needs "the per-receiver-CS demand of 129" to
  retire.
- **No object at all.** An iterator over a list is the pair (list,
  position). If iteration over a list were primitives over the list itself
  (`len` and `index_object` on a counter), there would be no iterator
  contour to merge. The loop variable's type would be `elem(L)` read from
  the loop's own list, and the four-pass lag would disappear by
  construction, with no identity rule involved. The obstacle is that the
  frontend lowers `for` before it knows the iterable is a list, so this is
  a dispatch question: a `__iter__` that resolves to `list.__iter__`
  becomes the primitive form. That is the same shape as ifa/134's
  logical-not primitive ("no contour at all").

The second is preferred, because it removes the confluence instead of
adding a rule about it. It only covers views the runtime can express as
primitives, but `list`, `tuple`, `str` and `range` iteration are most of
them.

### G3. Annotations, if any

PEP 695 syntax would be the natural spelling, in `__pyc__` only:

```python
class list:
  def __getitem__[T](self: list[T], key: int) -> T: ...
```

Annotations are parse-and-discard today (closed issues/016). Two reasons
not to start here:

- **G1 recognizes every case that matters for othello3 without
  annotations.** The paths are in the IF1.
- **An annotation FA obeys is the thing 134 is removing.** If annotations
  are added, they should be CHECKED, not trusted. At quiescence, an
  EntrySet's return must lie within its declared scheme instantiated over
  its own formals, and a mismatch is a compile error in `__pyc__`. That
  makes them documentation with teeth. Where they would add something real
  is `__pyc_c_call__`, whose return type is declared by hand today. User
  programs' annotations stay advisory, as in CPython.

## What it would do for othello3

| | pass 1 (184 s) | passes 2–6 | passes 7–14 |
| --- | --- | --- | --- |
| G1 | nothing: the union is in the start-merged list CreationSets, and every reader sees it through primitives already | possibly faster list separation (below) | each list's loop variable gets its own list's element type directly, but only once its iterator is separate |
| G2 (iteration as primitives) | nothing, for the same reason | loop variables take `elem(L)` from their own list as soon as `L` separates | the four-pass lag is gone, so convergence at about pass 10 instead of 14 |

Estimated, not measured: FA is ~850 s, passes 1–3 take ~410 s, and the
other eleven average ~40 s. Removing four of them saves ~160 s, so FA comes
to ~690 s and the compile to ~830 s, still over the 400 s cap. pass 1 is
untouched by either.

Unmeasured hypothesis: some of the one-or-two-per-pass list separation in
passes 2–6 waits on demands observed through shared builtin contours
(`join`'s `parts`, iterator contours). G1 and G2 would move those
observations to the merged contour, so lists might separate in fewer
passes. If so, the saving is larger. If not, it is not.

**What would get othello3 under the cap is a cheaper pass 1.** That pass
is the minimum-contour starting point carrying a 2,000-wide union through
tens of thousands of AVars. It is ifa/186's problem, and ifa/186's
remaining directions still apply. Generics are worth doing for what they
fix in general (unions that leave builtin contours, iterator lag, builtin-
internal containers), not as the othello3 fix.

## Plan, with stop conditions

1. **G2 for list iteration first.** It is the smallest change with a
   measured target. Measure othello3's pass count and the per-pass table
   above. **Stop if** convergence does not move at least three passes
   earlier: then the lag is not the iterator's, and G1/G2's model of
   othello3 is wrong.
2. **G1 for `__pyc__` projections** (`__getitem__`, `__next__`, `pop`,
   `get`, `iter`, and the type-independent ones like `len` and the
   comparisons). Measure with a corpus check sweep plus the `ess`/`css`
   totals. **Stop if** no program's pass count or EntrySet count moves:
   with start-merged lists, `elem(L)` may already be as merged as the
   contour return, and G1 would then be buying nothing at the default.
   **Investigate, do not retreat,** if a program's contour count rises (a
   demand that lost its observation point, see G1).
3. **Annotations only if** step 2 finds functions that recognition cannot
   see through (C calls, generated code), and only checked.

Related: [186](ifa/issues/186-FA-pass-one-confluence-through-shared-builtins.md)
(othello3), [129](ifa/issues/129-plan-demand-driven-creation-set-splitting.md)
(CreationSet splitting), [133](ifa/issues/closed/133-split-a-container-on-its-element-type.md)
("the demand is unobservable at the merge"),
[134](ifa/issues/134-remove-the-frontend-forced-split-opt-in.md) (the
annotations being removed), [146](ifa/issues/146-remove-all-arbitrary-splitting.md)
(why CPA is out).
