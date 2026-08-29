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
