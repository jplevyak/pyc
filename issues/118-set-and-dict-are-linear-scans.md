# 118 — `set` and `dict` are linear scans, so building one is O(n²)

**Status:** open, filed 2026-08-28. Diagnosed and a fix prototyped, then
**reverted** — the prototype is correct and fast but costs analysis
precision, for a reason that is itself the interesting part.
**Affects:** `__pyc__/08_set.py`, `__pyc__/07_dict.py`.
**Blocks:** `shedskin_examples/loop`.

## Symptom

`shedskin_examples/loop` compiles with **zero warnings on both backends**
and then runs past 300 s where CPython finishes in 64 s. Nothing in the
harness or in a compile-only sweep sees this.

## Cause

Both containers are linear scans over a list.

```python
class set:
  def __contains__(self, item):
    i = 0
    while i < self._len:
      if self._items[i] == item: return True
      i += 1
    return False
  def add(self, item):
    if not self.__contains__(item): ...
```

`dict.__getitem__`, `__setitem__`, `get` and `__contains__` are the same
shape over `_keys`. So every lookup is O(n) and building a container of n
entries is O(n²).

Measured — 20000 adds plus 20000 membership tests:

| | CPython | pyc |
|---|---|---|
| `set` | 0.00 s | **6.00 s** |

`loop`'s entire benchmark is sets of basic blocks plus two dicts
(`basic_block_map_` keyed by name, `number` keyed by *object*), at
15000+ nodes.

## The prototype, and why it was reverted

An open-addressed hash index over the existing storage — `_index` mapping
slot to position-plus-one, flat `list[int]` rather than buckets-of-lists
(a list-of-lists field on a class shared program-wide is exactly the
shape that unions element types across every container in the program),
grown at 50 % load, rebuilt after the shift in `discard`/`__delitem__`.
Plus `object.__hash__` returning `id(self)`, because `__hash__` existed
only on str/bytes/numeric/list/tuple and a set of class INSTANCES had
none.

It works, and it is as fast as it should be:

| | before | after |
|---|---|---|
| 20000 set adds + lookups | 6.00 s | **0.00 s** |
| 20000 dict stores + lookups | quadratic | **0.00 s** |
| int-, str- and object-keyed containers | — | output identical to CPython |

**It regresses the analysis, and the reason is structural.** Hashing has
to call `element.__hash__()`. A linear scan only ever uses `==`, which
pyc tolerates on a value whose type it does not know; a *dispatch* on
that value is not tolerated. So an EMPTY container — whose element type
the analysis has nothing to infer from (the [ifa/072](../ifa/issues/072-FA-empty-container-notype-current-mechanism-and-plan.md)
family) — now reports `'item' has no type`:

```
set_from_iterable.py:20: warning: 'item' has no type
    empty = set([])
  called from __pyc__.py:2708
```

Two suite tests regress (`set_from_iterable`, and
`list_index_type_mismatch_salvage`), plus `loop` itself goes 0 → 3
warnings on `for liter in loop.children_`, a set that is only ever
iterated and never added to.

**The dict half is worse than a warning.** With the hashed dict, `loop`
compiles and then computes the WRONG ANSWER (`Found 1 loops` against
CPython's `Found 76002`) and segfaults on a null `nodes[current]`.
Bisected: the hashed set alone gives the correct answer, the hashed dict
alone reproduces the failure, so it is the dict, not the set, and not
`object.__hash__`. Root cause not found.

## So the fix is gated on something else

This is not a drive-by. Hashing needs a container's element type to be
usable even when the container is empty, which is
[ifa/072](../ifa/issues/072-FA-empty-container-notype-current-mechanism-and-plan.md),
and it needs whatever makes the dict variant miscompile `loop` understood
first. Either would be worth doing on its own; together they are the
prerequisite for this.

The prototype is preserved at `wip_set_hashed.py` / `wip_dict_hashed.py`
in the session scratch and is straightforward to re-derive from this
description.

## Verification plan

- The set/dict micro-benchmarks above drop from 6.00 s to ~0.
- `shedskin_examples/loop` finishes and prints
  `Found 76002 loops (including artificial root node)(3800100)`.
- No new `has no type` warnings anywhere: `set_from_iterable` and
  `list_index_type_mismatch_salvage` stay clean, and `loop` stays at 0.
- `minmax_3arg.py.check` needs re-blessing whenever `__pyc__` line
  numbers shift ([issues/111](111-checks-embed-builtin-library-line-numbers.md)).

## How shedskin handles it — and it renames this issue's blocker

shedskin **builds and runs `loop` in 20 s** (CPython 64 s; pyc does not
finish in 300 s), printing the right answer:

    Found 76002 loops (including artificial root node)(3800100)

Its containers are C++ templates in the runtime, so its type analyser
never sees a hash function at all:

```cpp
template<class T> class set : public pyiter<T> { __GC_SET<T> gcs; ... };

using __GC_SET  = boost::unordered_flat_set<T,    ss_hash<T>, ss_eq<T>, ...>;
using __GC_DICT = boost::unordered_flat_map<K, V, ss_hash<K>, ss_eq<K>, ...>;
```

Hashing is `hasher<T>`, an overload set resolved at C++ compile time on
the STATIC type — the generic form for pointers plus explicit
specializations:

```cpp
template<class T> inline long hasher(T t) {
    if(t == NULL) return 0;
    return t->__hash__();          // virtual; pyobj::__hash__ is (intptr_t)this
}
template<> inline long hasher(__ss_int a)   { return std::hash<__ss_int>{}(a); }
template<> inline long hasher(__ss_float a) { return std::hash<__ss_float>{}(a); }
template<> inline long hasher(void *v)      { return std::hash<void *>{}(v); }
```

`pyobj::__hash__` returning `(intptr_t)this` is exactly the
`object.__hash__` the prototype added, so that half was right.

### The part that matters: an empty container is `void *`, not "no type"

`loop`'s own generated header:

```cpp
set<void *> *children_;                       // only ever iterated
dict<Basic_block *, __ss_int> *number;        // object-keyed
dict<__ss_int, Basic_block *> *basic_block_map_;
```

and `set([])` in a two-line program comes out `set<void *>` too. The
element type of an empty container is not unknown there — it is `void *`,
which has an explicit `hasher` specialization that always compiles.

So shedskin never has to dispatch a hash on a value whose type it does
not know, which is precisely what made the prototype warn. **That
reframes the blocker.** It is not "ifa/072 must be solved first"; it is
narrower and much more tractable: **the unknown/empty element type needs
a `__hash__` of its own** — pyc's analogue of `hasher(void *)` — so that
`item.__hash__()` resolves even when `item`'s type is unknown. Worth
trying before anything larger.

The dict miscompile (`Found 1 loops`, then a null `nodes[current]`) is
separate and still unexplained; shedskin's design says nothing about it.


## Tried it: the empty-container hash does NOT work, and here is why

Acting on the section above -- add pyc's analogue of `hasher(void *)` --
produced two measured results, one useful and one that corrects the
reframing itself.

**`object.__hash__` is a real fix on its own, and landed.** `__hash__`
existed on str/bytes/numeric/list/tuple but not on `object`, so `hash(x)`
of a class INSTANCE, or of a FUNCTION, compiled with one warning and then
died with `matching function not found`. `object.__hash__` returning
`id(self)` is CPython's own default and shedskin's
`pyobj::__hash__ { return (intptr_t)this; }`. Pinned by
`tests/hash_of_object_and_function.py`.

**`__pyc_any_type__.__hash__` was tried and REVERTED — twice wrong.**

1. It BREAKS what it was meant to help. With both defined, `hash(a)` on a
   plain instance becomes a multi-candidate dispatch and aborts:
   `hash(instance)` and `hash(function)` both worked with only
   `object.__hash__` and both failed with the pair. That is exactly the
   hazard `00_runtime.py` already records for hypothetical
   `__getitem__` / `__len__` stubs on `object` -- a stub turns sites that
   have one candidate into sites that have several.

2. It would not have reached the case that motivated it anyway. The
   warning is not on an *any-typed* value, it is on a value with **no
   type at all**:

       set_from_iterable.py:20: warning: 'item' has no type
           empty = set([])
         called from __pyc__.py:2648      <- set.update's `for item in other`

   pyc types an empty container's element as **bottom**. shedskin's
   `set<void *>` is a CONCRETE type with a hasher; bottom is not a type
   with a missing method, it is the absence of one, and no fallback
   method can attach to it.

So the shedskin comparison pointed at the **type lattice**, not at a
missing method: what is needed is for an empty container's element type
to BE something (ifa/072), at which point `object.__hash__` already
covers it. The previous section's "narrower and much more tractable" read
was wrong, and this one supersedes it.


## The dict miscompile, root-caused

Reproduced on a 5-second cut of `loop` (dummy loops 15000 -> 3, the two
CFG-building loops 10/100/25 -> 2/5/3), which CPython answers `Found 102
loops` in under a second.

**First, a diagnostic obstacle worth its own note.** The crashing binary
printed NOTHING, which made it look as though it died before `main`.
`_CG_Syscall_Write` is `fwrite(..., stdout)` — **buffered** — so a
segfault loses every line the program produced. `stdbuf -o0` gets it
back, and every runtime failure in this corpus is easier to diagnose
with it.

With output restored, instrumenting `dfs`:

```
SIZE 11
DFS 0 11
  set-> -1 len 12        <- number[current_node] = current did not take
DFS 1 12
  set-> -1 len 13
```

`number[current_node] = current` **appends a duplicate instead of
overwriting**: `len` climbs 11, 12, 13, … and reading the key back still
gives the old `-1`. The `K_UNVISITED` guard in `dfs` therefore never
clears, the DFS revisits nodes forever, `current` runs past
`size = len(basic_block_map_)`, and `nodes[current]` — a list of exactly
`size` elements — hands back NULL. `Union_find_node::init(NULL, ...)`
segfaults. That is the whole chain, and `Found 1 loops` is the same cause
seen from the other end.

Instrumenting `_slot` shows why the overwrite misses:

```
DFS 0 11
   probe i= 0 p= 1 eq= False
   probe i= 1 p= 2 eq= False
   ...
   probe i= 10 p= 11 eq= False
SET h= 123944734424032 mask= 31 slot= 11 p= 0 len= 11
```

**Two independent defects.**

1. **Every key lands in bucket 0.** The probe starts at `i = 0` with
   `mask = 31` for pointer-valued hashes that are certainly not
   0 mod 32. The generated C says why:

       t57 = _CG_f_183_135/*__pyc_any_type__::__hash__*/(t48);
       t55 = _CG_prim_and(t57, _CG_Symbol(7588, "&"), t49);

   `key.__hash__()` dispatches to the TOP TYPE's hash because `_keys`'
   element is a union (`{int64, Basic_block, ...}` — `loop` has an
   int-keyed dict and an object-keyed one, and they share one
   CreationSet), and `&` on that result yields 0 every time. The table
   degenerates to one probe chain. Slow, not yet wrong.

2. **`self._keys[p - 1] == key` is False even against the identical
   object** — `eq= False` on all eleven probes, one of which compared the
   key with itself. THIS is the wrong answer. The emitted comparison
   branches on the type tag (`== &_CG_type_Basic_block`,
   `== &_CG_type_Union_find_node`); reading the key back out of the union
   slot evidently does not present the tag the comparison expects, so
   every branch falls through to False.

The linear dict compares the *same* expression, `self._keys[i] == key`,
and gets it right — so this is not "== on a union is broken" in general.
What differs is that `_slot` is a separate method whose `self._keys[...]`
read is typed in its own contour. That is where to look next.

Not fixed. But "root cause not found" is no longer accurate: the failure
is a union-typed key slot that neither hashes nor compares as itself, and
either defect alone is enough to lose the entry.


## The actual root cause: an uninitialized method slot

The section above stopped at two symptoms and called them the cause. They
are not. Instrumenting the probe with identities settles it:

```
DFS 0 11
   probe i= 0 p= 1 stored_id= 136251752655840 key_id= 136251752655840 eq= False
```

**Same pointer, `eq= False`.** So this is not about hashing, unions, or
`_slot`'s indexing: `a == a` on one object returns False.

`object.__eq__` is `__pyc_primitive__("is", self, x)` and its emitted
body is a correct pointer compare:

```c
_CG_bool _CG_f_307_3/*object::__eq__*/(_CG_any a1, _CG_any a2) {
  t1 = ((void *)t2 == (void *)t3);   /* right */
```

The defect is in **how it is reached**. Because `_keys`' element is a
union of two user classes, codegen emits a vtable-style dispatch through
the instance's method slot rather than a direct call:

```c
if ((*(_CG_TypeObject**)(void*)t70) == &_CG_type_Basic_block) {
  t62 = ((_CG_bool(*)(void*, _CG_any))((_CG_ps14653)(void*)t70)->e1)((void*)t70, ...);
```

`e1` is the `__eq__` slot, and **it is never written for `Basic_block`**.
Grepping every `->e1 = ...__eq__` assignment in the generated C: the
builtin prototypes get one, and among the user classes
`Basic_block_edge` (`_CG_ps14631`) gets one — `Basic_block`
(`_CG_ps14632`) does not. Its prototype is `_CG_prim_new`'d (so the slot
is zero), `__pyc_tag` is set, `___init___` runs, and every instance is
cloned from it. The dispatch calls through that slot anyway.

That is `cg_build_new_to_val_map` in `ifa/codegen/codegen_common.cc`. It
registers a class's slot by tracing "the FA creation chain from that
arg's AType through `cs->defs` to the creator function"; for
`Basic_block` that trace does not produce a creator, so no assignment is
emitted. The two earlier symptoms follow from the same site being
polymorphic: the hash dispatches to `__pyc_any_type__::__hash__` (so
`&mask` is 0 and every key shares a bucket), and the comparison dispatches
through the empty slot (so nothing ever matches).

**Why the linear dict is unaffected**: its `==` sites are monomorphic
per contour, so codegen emits a DIRECT call to `object::__eq__` and never
reads the slot. The hashed version did not introduce a new kind of
comparison — it introduced a new POLYMORPHIC one, and that is what
exposed the missing slot.

So this is not really a dict bug at all. It is a codegen bug --
a polymorphic dispatch site whose method-slot table is incomplete for
user-defined classes -- and it is closely related to
[ifa/110](../ifa/issues/closed/110-override-duplicates-member-slot.md)'s
family of member-slot problems. Anything else that makes a user class
reach a polymorphic `__eq__` will hit it without any hashing involved,
which is worth a reproducer of its own.


## Is the dispatch even necessary? No -- it is an artifact

`loop` contains exactly **two** dicts:

```python
number = {}                    # keys: Basic_block
self.basic_block_map_ = {}     # keys: int
```

and `Union_find_node` appears **only in a list** --
`nodes = [Union_find_node() for _ in range(size)]` -- never as a dict
key. So **no `==` receiver in the program is genuinely polymorphic over
`{Basic_block, Union_find_node}`.** The union is manufactured by the
analysis: `dict._keys` is one `list` field on a class shared
program-wide, so its element type collects from every dict (and, being a
list, from the program's other lists too). That is
[ifa/105](../ifa/issues/105-type-degeneration-in-shared-generic-methods.md)
and [issues/039](039-list-mul-shared-element-type-cross-contamination.md),
the same shared-CreationSet degeneration as everywhere else.

With precise contours `_slot`'s `==` would be monomorphic, codegen would
emit a direct call, the uninitialized slot would never be read, and there
would be no bug.

### shedskin resolves it monomorphically, by construction

```cpp
dict<Basic_block *, __ss_int> *number;
dict<__ss_int, Basic_block *> *basic_block_map_;
list<Union_find_node *> *nodes;
```

`dict<K,V>` is keyed on `K`, so those are two separate template
instantiations that cannot contaminate each other. The key comparison is

```cpp
template<class T> class ss_eq {
    bool operator()(const T a, const T b) const { return __eq<T>(a, b); }
};
template<class T> inline __ss_bool __eq(T a, T b) {
    return ((a&&b) ? (a->__eq__(b)) : __mbool(a==b));
}
```

-- `T` is statically `Basic_block *`, so `a->__eq__(b)` is an ordinary
C++ virtual call on a known class. No type-tag switch, no slot table to
populate, nothing to leave empty.

### Which means there are two independent fixes

Either one unblocks the hashed dict:

1. **Populate the slot.** `cg_build_new_to_val_map` must cover every
   (class, method) pair reachable from a polymorphic site. This makes the
   polymorphic path CORRECT, and is worth doing regardless -- the bug is
   reachable from any program that gets a user class to a polymorphic
   `__eq__`, hashing or not.
2. **Remove the polymorphism.** Split `dict._keys` per dict contour, so
   the site is monomorphic and the slot is never consulted. This is what
   ifa/105 has wanted all along, and it is also what would make the
   hashing FAST rather than merely correct -- the same union is why the
   hash dispatches to `__pyc_any_type__::__hash__` and every key lands in
   bucket 0.

Fix 1 alone gives a correct but degenerate hash table. Fix 2 alone gives
a fast and correct one, and retires the codegen bug's reachability here
without fixing it.


## Removing the polymorphic split: it works, and the split was self-inflicted

Asked to attack the union rather than the codegen slot. Three
measurements of the same list CreationSets on `loop`
(`IFA_DBG_ELEMTYPE_DUMP`), and they settle it:

| dict implementation | element types |
|---|---|
| linear (shipped) | `[ Union_find_node ]`, `[ Basic_block ]` — **monomorphic** |
| hashed, with a `_slot(self, key)` helper | `[ int64 __pyc_None_type__ Basic_block Union_find_node ]` |
| hashed, probe **inlined** into each caller | monomorphic again |

So the union was not pre-existing and it was not `loop`'s: **the helper
method created it.** `_slot(self, key)` is one more shared generic method
on a class every program instantiates, and its `key` parameter merges
every dict's key type into one contour — [ifa/105](../ifa/issues/105-type-degeneration-in-shared-generic-methods.md)'s
mechanism exactly, introduced by the very code that then tripped over it.

**Inlining the probe fixes the miscompile.** The reduced `loop` now
prints `Found 102 loops (including artificial root node)(5100)` — CPython's
answer — where the `_slot` version printed `Found 1 loops` and segfaulted.
The full `loop` gets from an immediate crash to running normally through
its "Another 50 iterations..." phase.

Four copies of an eight-line probe is the price. A C++ template would
duplicate them too; the difference is that shedskin's duplication is per
instantiated key type and this one is per call site.

### What still blocks it

1. **The untyped key, again.** Six tests regress, all the same shape as
   the set's:

       dict_from_iterable.py:21: warning: 'pair' has no type
           empty = dict([])

   `dict([])` and `set([])` have nothing to infer a key/element type
   from, and hashing must call a method on it. Same wall, same issue
   (ifa/072), now confirmed on both containers.

2. **The full `loop` needs a bigger C stack.** Its DFS recurses ~16000
   deep (the program itself calls `sys.setrecursionlimit(100000)`); at
   the default 8 MB the compiled binary overflows, and `ulimit -s
   1048576` turns the segfault into a normal run. CPython heap-allocates
   frames and does not care. That is a separate limitation, not this
   issue.

3. **The hashed dict alone is not enough to make `loop` fast.** With the
   stack raised it runs correctly but reached only 27 of its 50
   iterations in 280 s — still slower than CPython's 64 s for all 50.
   That is the `set` half: `non_back_preds[w].add(v)` over thousands of
   nodes is still the O(n^2) linear scan, since only the dict was hashed
   for this experiment. `loop` needs BOTH containers, which is what this
   issue said at the top and is now measured from both ends.

The inlined implementation is preserved at `dict_hashed_inlined.py` in
the session scratch. It is correct and fast; only the untyped-key
warnings keep it out of the tree.

### And the codegen bug does not go away

Removing the split makes the bad dispatch unreachable *here*, but
`cg_build_new_to_val_map` still fails to write `Basic_block`'s `__eq__`
slot. Any other program that reaches a polymorphic `__eq__` on a user
class hits it. That deserves its own reproducer and its own fix.


## Tried to build a minimal repro for the codegen bug. It does not exist standalone.

Four constructed programs, each a polymorphic `==` over two user classes
with no `__eq__` override:

| program | result |
|---|---|
| two classes, mixed list, compare each against one element | correct |
| same, built through a factory method and stored in a dict | correct |
| same, with different field counts | correct |
| same, with monomorphic comparisons first to force per-type clones | correct |

Every one emits a **direct call** to a SINGLE clone of `object::__eq__`
whose first parameter is `_CG_any`:

```c
t10 = _CG_f_307_0/*object::__eq__*/(t11, t12);
```

Not one of them contains a type-tag dispatch (`== &_CG_type_`) at all, so
the method slot is never consulted and its being unwritten costs nothing.
Grepping confirms it: `->e1 = ...__eq__` appears ZERO times in those
programs' generated C, and they still give the right answer.

The failing configuration is different in exactly one measurable way:

    loop, hashed dict with _slot()   object::__eq__ clones: 2
                                     type-tag dispatches:  18

**Two clones of the same method reaching one call site is what turns on
the slot path** — `cg_build_new_to_val_map`'s pass 1 collects method
names where `fns->n > 1`. A single generic `_CG_any` clone never needs a
vtable.

### So the two fixes are NOT independent, and I said they were

The earlier claim that "any other program that reaches a polymorphic
`__eq__` on a user class will hit it" is not supported. The slot path is
reached only when FA has split `object::__eq__` into several clones, and
the only way seen to do that is the contour degeneration the shared
`_slot` method caused. **Removing the split removes the only known route
to the codegen bug.**

That reorders the work. Fix 2 (split `dict._keys` per contour, or simply
do not add shared generic helpers to the builtin containers) is the one
to do: it makes the hashing correct AND fast, and it retires the codegen
path rather than merely making it correct. Fix 1 stays worth doing on its
own terms — an unwritten slot behind a dispatch is a latent bug whatever
reaches it — but it is not a prerequisite, and there is no test to write
for it yet.


## Fix 2, attempted both ways

**Way A — do not create the shared helper (inline the probe). DONE, and
it works.** Measured above: it removes the union and fixes the
miscompile. The rule is now written up in
[RUNTIME.md](../RUNTIME.md#do-not-add-a-shared-helper-method-to-a-builtin-container-class),
since it applies to any future edit of `dict`/`set`/`list`, not just to
hashing.

**Way B — mark `dict`/`set` `clone_methods_per_cs`, so each instance
gets its own field CSs.** Prototyped behind `PYC_PERCS_CONTAINERS` and
**reverted: it changes nothing measurable here.** The existing route to
that flag is a `__pyc_clone_constants__` ctor parameter (ifa/045), which
neither class can reach since neither takes ctor arguments, so the flag
had to be set by class name. With the probe already inlined, `loop` is
correct with and without it (3 warnings either way, same output), and it
does not touch the remaining blocker at all. A knob in this codebase is
supposed to carry a measurement that justifies it; this one has none, so
it is not in the tree.

### What is left is not fix 2

The hashed containers still cannot land, and the reason is unchanged and
orthogonal to the split:

    dict_from_iterable.py:21: warning: 'pair' has no type
        empty = dict([])

`dict([])` and `set([])` have no element type to infer, hashing must call
a method on the key, and a bottom-typed value has no method. Per-CS
splitting cannot help — an empty dict's own `_keys` is still empty. This
is ifa/072 and nothing else, now demonstrated from three directions
(set, dict, and the per-CS experiment).


## Fix 1 done: a method may only claim a class's slot if it OWNS that class

Worth doing after all, and the reproducible failing configuration (the
`_slot`-helper hashed dict) made it verifiable end to end.

`IFA_DBG_POLYSLOT` (added) reports every registration and rejection in
`cg_build_new_to_val_map`. On the failing build:

    [polyslot] __eq__ fun=37 selfunion=0 selftype=Basic_block
                : cs Basic_block slot 1 defs=1 es_defs=1 live=1

and yet the emitted `Basic_block::__new__` contained

    ((_CG_ps14509)t1)->e1 = (void*)_CG_f_431_6/*__pyc_None_type__::__eq__*/;

**The slot was not empty. It held the WRONG method.** `__pyc_None_type__
.__eq__` is None's identity compare -- it answers False for every
non-None argument -- so a dict never matched its own key, and the earlier
write-up's "uninitialized slot" was a misreading of the same evidence.

It got there because a clone's self AType carries every CreationSet that
reached it. With `_keys` degenerate, None's `__eq__` had a self typed
`{nil, Basic_block, ...}`, so it registered itself as `Basic_block`'s
`__eq__` and won the specificity tie-break.

**The guard**: a clone may only implement a class's slot when the class
it was DECLARED on -- the method Sym's self formal's `must_specialize`,
the same thing `assign_fun_cg_strings` prints as the `Class::` half of a
clone's name -- is that class or an ancestor of it.

```cpp
Sym *owner = (fun_val->sym->has.n > 1) ? fun_val->sym->has[1]->must_specialize : nullptr;
if (owner && cs->sym != owner && !owner->specializers.set_in(cs->sym)) continue;
```

`object::__eq__` still registers for everything (object is everyone's
ancestor), and an inherited method with a `Type_SUM` self still registers
for the members that specialize its owner, which is issue 026's case.

Verified against the failing configuration: the reduced `loop` with the
`_slot` hashed dict goes from `Found 1 loops` plus a segfault to
`Found 102 loops (including artificial root node)(5100)` -- CPython's
answer -- and the bad slot write is gone. Five CI gates green,
305 / 0 / 14 on both backends.

**So both fixes are now done**, by different routes: fix 2 removes the
degenerate union that manufactures the bad receiver, fix 1 makes the
dispatch correct even when a degenerate union exists. Neither is in the
tree as a *hashed container*, because what still blocks that is
[ifa/072](../ifa/issues/072-FA-empty-container-notype-current-mechanism-and-plan.md)
and only that.

No standalone reproducer for fix 1 exists -- see the section above; a
single generic `_CG_any` clone never needs a vtable, so no small program
takes the slot path. The regression risk is covered by the suite, which
exercises the polymorphic-dispatch tests (`poly_dispatch_low/high`,
`method_override_field_offset`, the issue-026 inheritance cases) that this
code was built for.
