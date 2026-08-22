# 110 — `tuple(iterable)` returns a list

**Status:** open, found 2026-08-19 while tracing `sunfish`'s runtime
abort. Repro: `tests/tuple_from_iterable_is_list.py` (`.known_issue`).

## Symptom

```python
row = [1, 2, 3]
padded = (0,) + tuple(x + 1 for x in row) + (0,)
print(padded)
```

| | result |
|---|---|
| CPython | `(0, 2, 3, 4, 0)` |
| **pyc** | **`[0, 2, 3, 4, 0]`** — a list |

## Cause — a documented compromise

`python_ifa_build_if1.cc`:

```cpp
// Established compromise: zip/map/filter/enumerate/reversed
// already return lists; indexing/iteration/len are identical,
// printing/hashing differ.
if (f == sym_tuple && pos_args.n == 1) {
  call_method(&ast->code, ast, a0->rval, make_symbol("__pyc_tolist__"), ast->rval, 0);
}
```

A one-argument `tuple(x)` is lowered to `x.__pyc_tolist__()`.

## Why it is worth revisiting now

1. **The output is visibly wrong.** `printing/hashing differ` understates
   it: any program that prints a `tuple(...)` result prints a list.
2. **It manufactures a `{list, tuple}` union.** `sunfish`'s
   `padrow = lambda row: (0,) + tuple(x+piece[k] for x in row) + (0,)`
   yields a *list*, while its `pst` values also come from tuple literals
   and `()`. `pst[k]` therefore holds both, and any shared method on that
   union — slice, `len`, index, iterate — has two candidates with the
   same C-level receiver type and no runtime tag, so it aborts
   (`tests/list_tuple_union_method.py`,
   [030](../ifa/issues/030-DISPATCH-polymorphic-dispatch-fat-pointers.md)).
   shedskin types the whole program `dict<str*, tuple<__ss_int>*>` with
   **no union at all**, precisely because its `tuple(iterable)` returns a
   tuple.
3. **The blocker that motivated the compromise is gone.** Returning a
   real tuple used to be impossible for an iterable of unknown length,
   because pyc's tuples were fixed-arity records. As of
   [ifa/issues/109](../ifa/issues/109-mixed-arity-tuple-slice-dispatch.md)
   a tuple CreationSet whose generic element is populated takes **list
   layout** — variable length, known element type. So a genuine
   variable-length tuple is now representable.

## Fix direction

Lower `tuple(x)` to something that produces a **tuple** whose CreationSet
takes list layout — the same representation `tuple.__pyc_getslice__` now
yields — rather than to `__pyc_tolist__`. The runtime layout is already
shared (`cg.cc` builds every tuple with `_CG_prim_tuple_list`, which sets
a real list header), so this is about the *type* the frontend assigns,
not about storage.

Watch the same trap 109 hit: `sizeof_element` needs the element sym,
which `PYC_TUPELEM` (now default) supplies.

## Verification plan

- `tests/tuple_from_iterable_is_list.py` prints `(0, 2, 3, 4, 0)` and
  `(0, 2)`; delete its `.known_issue` tag.
- `tests/list_tuple_union_method.py` is unaffected (it builds its union
  explicitly, so it stays a genuine 030 case).
- `sunfish` compiles **and runs**.
- Corpus: no exit-code changes, measured against a freshly taken
  baseline. The named beneficiaries `genetic2` (`tuple([TreeNode() ...])`)
  and `chess` (`tuple(range(...))`) must not regress — they are the two
  the compromise was originally made for.


## Attempted 2026-08-19 — the library-level construction does not work

The plan was to keep the frontend intercept's shape and only change what
it dispatches to: `tuple(x)` → `x.__pyc_totuple__()`, with

```python
def __pyc_totuple__(self):
    t = ()
    for x in self:
        __pyc_primitive__(__pyc_symbol__("merge_in"), t, x)   # flow element types in
    return __pyc_c_call__(__pyc_primitive__(__pyc_symbol__("merge"), t, t),
                          "_CG_list_getslice", list, self, ...)
```

The reasoning was that `merge_in` (the primitive `append` uses) would
populate the empty tuple's generic element, making `tuple_able()` false
and so giving the CreationSet list layout — variable length, known
element type — with `merge(t, t)` naming that tuple type as the return.

**It compiles and is wrong**: the reproducer prints `[0, 0]` against
CPython's `(0, 2, 3, 4, 0)` — still a list, and now with wrong values
too. `merge(t, t)` on an empty tuple literal names the *empty-tuple*
type; populating an element through `merge_in` did not turn it into the
variable-length tuple the return needs, and the copy went wrong as well.
Reverted; baseline restored and re-verified.

### What that tells us

The blocker is not the *representation* — ifa/issues/109 established that
a tuple CreationSet with a populated element does take list layout, and
`tuple.__pyc_getslice__` returns exactly such a value today. The blocker
is **constructing a value of that type from `__pyc__` Python source**:
there is no expression that names "tuple, variable length, element type
E". A tuple literal names a fixed arity; `merge`/`merge_in` on one does
not generalise it.

So this needs a frontend or FA-level construction — a primitive that
makes a tuple CreationSet whose element is seeded from an iterable's
element — rather than a `__pyc__` library edit. That is the same shape as
`P_prim_make`, which builds a tuple from a *fixed* argument list; what is
missing is its dynamic-length counterpart.

Estimated properly this time: a new primitive plus its FA constraint and
codegen, not a library change.

## Why not another `__pyc_primitive__`? — that IS the answer, but it must be a real one

Three library formulations were tried and all produce the same wrong
output (`[0, 0]` against CPython's `(0, 2, 3, 4, 0)`):

1. slice `self` directly with `_CG_list_getslice`, return typed
   `merge(t, t)` for `t = ()`;
2. **iterate** to collect first (fixing the fact that `self` may be a
   generator, which is not indexable), then slice the collected list;
3. iterate *and* `merge_in(t, x)` each element, to populate the result
   tuple's generic element so its CreationSet would take list layout.

All three fail for one reason: **`t = ()` names the empty-tuple type,
which is a record with arity 0**, so the result is empty. `merge`,
`merge_in` and iteration do not generalise a fixed-arity tuple type into
a variable-length one.

### The primitives that already exist do not cover it

| primitive | what it is |
|---|---|
| `make` (`prim_make`, 36) | builds a container from a **fixed** argument list — the tuple literal path |
| `clone` (`prim_clone`, 39), `copy` (`prim_copy`, 58) | real IFA primitives; produce a **fresh CreationSet of the same sym**, so they preserve arity rather than generalising it |
| `merge` / `merge_in` | type-level union / element flow; neither changes a record's arity |
| `make_tuple` | **not an IFA primitive at all** — `c_runtime.h` maps it to `_CG_symbol`; `__pyc_tuplify__`'s use is unrelated |

So there is no expression, in `__pyc__` Python source, that names *"tuple,
variable length, element type E"*. Every tuple-typed expression available
is a literal with a fixed arity.

### What the new primitive has to be

The dynamic-length counterpart of `P_prim_make`: given an iterable, make
a **tuple CreationSet whose generic element is seeded from that
iterable's element**. Populating the element is exactly what makes
`tuple_able()` false, so `clone.cc` gives the CreationSet list layout —
variable length, known element type — which is the representation
`tuple.__pyc_getslice__` already returns today
([ifa/issues/109](../ifa/issues/109-mixed-arity-tuple-slice-dispatch.md))
and which shedskin spells `tuple<T>`.

That needs three pieces, none of them large but none of them expressible
in the library:

1. a `Prim` entry alongside `prim_make`;
2. an FA constraint that creates the CreationSet and flows the source's
   element into its element AVar (the shape of `make_kind`, but with the
   element rather than per-index vars);
3. codegen, which is nearly free — `cg.cc` already builds every tuple
   with `_CG_prim_tuple_list`, so the storage and header are identical to
   a list's.

The library attempts are recorded here so the next attempt starts from
the primitive rather than rediscovering that the library cannot express
this.

## The primitive: built, working, and 3 tests short of shippable

`make_seq` now exists — `PYC_MAKESEQ=1`, off by default.

### What was added

| piece | where |
|---|---|
| `Prim` registration (index 61, freed by issue 069) | `ifa/if1/prim_data.{h,cc}` |
| FA constraint — mint the CreationSet, flow the source's element into its element | `fa.cc`, `P_prim_make_seq` |
| codegen — copy the storage with the list slicer | `cg.cc` |
| frontend — `tuple(x)` emits it instead of `__pyc_tolist__` | `python_ifa_build_if1.cc` |

One wrinkle worth recording: a `builtin_symbols.h` entry (`sym_make_seq`)
does **not** work — those are bound from the builtin AST and a pyc-only
primitive is not in it, so `finalize_types` asserts. Reference it by name
via `make_symbol("make_seq")`, exactly as `isinstance` and `merge_in` are.

### It works

```
tuple(x + 1 for x in row)  ->  (2, 3, 4)      # was [2, 3, 4]
tuple([7, 8])              ->  (7, 8)         # was [7, 8]
```

both matching CPython. `tests/builtin_type_factory.py` also changes from
`[1, 2, 3]` to `(1, 2, 3)` — **CPython prints `(1, 2, 3)`**, so that
test's `.check` had been recording the bug.

### But it regresses three tests

| test | |
|---|---|
| `builtin_type_factory` | now **correct**; its `.check` encodes the old buggy list output and needs regenerating |
| `list_element_type_union` | **real regression** — prints `0 0 1 / 1 0 1 / 2 0 1` against `1 125 141 / 2 576 717` |
| `list_hash` | **real regression** |
| `deepcopy_objects` | **real regression** — C compilation failure |

An earlier version also aborted the compiler on `genetic2_idioms`
(`compute_setters`' `x->contour_is_entry_set` assertion) because the FA
constraint flowed a record-shaped source's **per-index vars** into the
element; those AVars are CS-contoured. Removing that fixed `genetic2` and
is why `tuple(a_fixed_tuple)` currently contributes no element type — a
known gap in this implementation.

### Where it stands

The design is validated: a dynamic-length tuple **is** representable and
the primitive produces one. What is not done is the fallout — three
regressions that are presumably element-type flow being too narrow (the
element now comes only from the source's generic element, so a source
whose element is unpopulated yields an untyped result).

Kept behind `PYC_MAKESEQ` rather than reverted, because the hard part —
the primitive, its FA constraint and codegen — is done and working, and
the remaining work is diagnosing three specific tests. Default is
unchanged: **275 passed / 17 known / 0 failed**.

## Diagnostics (2026-08-19): 4 failures → 2, and the core case is exact

### 1. The two flags are interdependent — `PYC_MAKESEQ` needs `PYC_TUPLE_AS_LIST`

The first symptom was `runtime error: bad getter` on `t[0]`. The emitted
guard is `t->type_kind == Type_RECORD && !t->has.n` — the result was a
**record with zero fields**. Cause: `define_concrete_types` computes
`tuple_is_record = sym == sym_tuple && !tuple_as_list_enabled()`, and
`PYC_TUPLE_AS_LIST` defaults **off**, so every tuple takes the record
branch *regardless of its element*. The new CreationSet has a populated
element and still got record layout.

With both flags the core case is byte-exact against CPython:

```
len 3 | t[0] t[1] t[2] = 1 2 3 | print(t) -> (1, 2, 3)
hash(t) == hash(tuple([1,2,3]))  True
hash(t) == hash(tuple([1,2,4]))  False
```

That also fixed `list_hash`, which had been failing because
`hash(tuple([1,2,3])) == hash(tuple([1,2,4]))` — the hash was reading a
zero-field record.

### 2. Literal sources have a bottom element — `update_gen`, not `flow_vars`

`tuple([1,2,3])` initially produced `sizeof(_CG_void_type)`. The source is
a **list literal**, and by the `tuple_able` design `make` fills per-index
vars and leaves the generic element bottom — so there was nothing to
flow. The element must come from the per-index vars.

It must be contributed with `update_gen`, **not** `flow_vars`: those AVars
are CS-contoured, and a flow *edge* into an element var trips
`compute_setters`' `x->contour_is_entry_set` assertion (measured:
`genetic2_idioms` aborted the compiler). `update_gen` contributes the
type without creating a setter. After that, `sizeof(_CG_int64)`.

### 3. What is left: two genuine failures, both element-type gaps

| test | status |
|---|---|
| `builtin_type_factory` | **not a regression** — now prints `(1, 2, 3)`, which is what CPython prints. Its `.check` records the old buggy `[1, 2, 3]` and can only be regenerated once this is the default. |
| `deepcopy_objects` | `tuple([leaf1, leaf2])` — a list of **user objects**. Element resolves to `_CG_void`, so codegen emits `sizeof(void)`. |
| `list_element_type_union` | `tuple(self.state[20:32])` — a tuple of a **slice**. Compiles, wrong values. |

Both are the same shape: the element type is not reaching the result for
sources that are not plain literals of scalars — a slice result, and a
list of records. The `update_gen` from `scs->vars` covers a literal; it
evidently does not cover these.

That is the next thing to fix, and it is one constraint, not a redesign.

Default remains **275 passed / 17 known / 0 failed**; with both flags,
273 passed / 3 failed (one of which is the corrected output).

## Constraint extension attempt (2026-08-19): the two failures are not the element

Both minimal forms of the suspected shapes **work**:

```python
t = tuple([a, b])          # list of user objects  -> 2 1 2, matches CPython
t = tuple(s[1:3])          # tuple of a slice      -> 2 2 3, matches CPython
```

So the diagnosis in the previous section — "the element type does not
reach the result for non-scalar-literal sources" — is **wrong**. The
element flow is fine for both shapes in isolation.

### What `deepcopy_objects` actually hits

Its shape is `T(0, 5, None)` alongside `T(1, 0, tuple([leaf1, leaf2]))`,
so `node.args` is a `{None, tuple}` union, and `copy.deepcopy` is then
applied twice. The generated C is:

```c
_CG_void t3;
t1 = (_CG_void)_CG_prim_copy_dst(_CG_void, t2);
```

The failure is not a void **element** — it is a void **type**: the copy's
whole destination type is unresolved. `structural_assignment` does carry
the element across a copy (`flow_vars(elem, tval)` →
`flow_vars(tval, get_element_avar(new_cs))`, guarded on
`new_cs->sym->element`, which `PYC_TUPELEM` supplies), so the element
machinery is not the gap either.

That points at the `{None, tuple}` union rather than at `make_seq`: a
deepcopy whose source is `None`-or-container is the
[018](018-dict-mixed-key-types-boxing-failure.md) shape, and the new
list-layout tuple changes which side of it resolves.

### Where this leaves 110

Not a one-constraint fix, and the earlier estimate that it was should be
discounted. `make_seq` itself is sound — it produces a real
variable-length tuple that matches CPython on `len`, indexing, `repr` and
`hash`, and the two failing tests fail on *interactions* (a
`{None, tuple}` union through deepcopy; a slice inside a larger program)
rather than on the primitive.

Both flags stay off. Default: **275 passed / 17 known / 0 failed**.
The next step is to reduce `deepcopy_objects` the way `plcfrs` was
reduced — against the invariant "still emits `_CG_prim_copy_dst(_CG_void,
…)`" — rather than to keep extending the constraint on a hypothesis that
the minimal cases have already falsified.

## Reduction (2026-08-19): `deepcopy_objects` reduced 56 → 7 lines

Mechanical reduction against the invariant *"still emits
`incomplete type 'void'`"*, with the same oracle discipline that the
`plcfrs` work settled on (parses, no unbound names, runs clean under
CPython with output, then the target failure). 56 → 23 lines; hand-trimming
the class the line-granularity reducer could not remove took it to **7**:

```python
import copy
class T:
    def __init__(self, args=None):
        self.args = args
tree = T(tuple([T(None)]))
c = copy.deepcopy(tree)
print(c.args[0].args)
```

Landed as `tests/deepcopy_none_or_tuple_field.py`. It **passes at the
default settings**, so it is not `.known_issue`-tagged — it is there to
pin the shape that blocks defaulting the flags.

### What it shows

`args` is `None` on the leaf and a tuple on the root, so deepcopy's
generic copy sees a **`{None, tuple}` union** and resolves the
destination to nothing:

```c
_CG_void t3;
t1 = (_CG_void)_CG_prim_copy_dst(_CG_void, t2);
```

That is the [018](018-dict-mixed-key-types-boxing-failure.md) union shape
reached through `copy`, **not** a defect in `make_seq` — `tuple(...)` on
its own matches CPython on `len`, indexing, `repr` and `hash`. What the
new list-layout tuple changes is which side of that union resolves.

### Things the reduction settled that guessing had not

- **`count()` is not needed** — the recursion over `args` was incidental.
- **The `Node` class is not needed.** The reducer kept it only because
  removing it line-by-line broke syntax; deleting the whole class by hand
  still reproduces. Worth remembering: a line-granularity reducer cannot
  remove a *class*, so its output overstates what is essential.
- **The copy-of-copy is not needed** — one `deepcopy` suffices (it halves
  the error count, 4 → 2, but the failure remains).

So 110's remaining blocker is precisely: *deepcopy of a record field
typed `{None, container}`*. That is a pre-existing 018 case that the
list-layout tuple makes reachable, and it is the thing to fix — or to
confirm as acceptable — before `PYC_MAKESEQ` and `PYC_TUPLE_AS_LIST` can
default on.

Suite **276 passed / 17 known / 0 failed**.

## How shedskin handles it — and a correction to the blocker's diagnosis

shedskin translates and **builds** the 7-line reproducer with no warnings,
and prints `None`. Its typing:

```cpp
class T : public pyobj {
    tuple<T *> *args;
};
```

**`args` is a plain pointer, and `None` is a null pointer of that type.**
So `{None, tuple}` needs no union at all — the null pointer *is* the
`None` case. That works because both members are pointer-represented,
which is exactly the distinction
[052](closed/052-llvm-nil-test-on-scalar-union-prints-none-for-zero.md) turns on:
`{None, int}` is unrepresentable this way (0 is a valid `int`), while
`{None, container}` is free.

### pyc already does this — for lists

| | default | flags on |
|---|---|---|
| `{None, list}` field, deepcopied | **compiles, prints `None`** | — |
| `{None, tuple}` field, deepcopied | **compiles** | **FAILS** |

So pyc handles `{None, container}` correctly when the container is a
`list`. The failure appears only for a **list-layout tuple**.

### That corrects the previous section

The blocker is **not** a pre-existing 018 gap that the new tuple merely
exposes — 018 would have to break the `list` case too, and it does not.
It is an **incompleteness in the tuple-as-list work**: a tuple that took
list layout is not inheriting the copy-path handling a real list gets, so
`_CG_prim_copy_dst` resolves its destination to `_CG_void`.

That is a much better-scoped problem than "fix 018 first", and it is
squarely inside this issue rather than blocked on another. The place to
look is what `define_concrete_types` produces for a list-layout tuple
versus a list — `sym_list` and `sym_tuple` are both `Type_PRIMITIVE`, so
the two should be taking the same path, and evidently are not.
## Root cause found: a snapshot read, one pass too late

Fixed in `50ebc8aa`. The previous section was wrong too, and in an
instructive way: there was never a copy-path problem at all.

`sym_list` and `sym_tuple` really are both `Type_PRIMITIVE`, and the two
really do take the same path. What differed was the *input* to that path.

### The measurement

The `deepcopy` framing was a red herring. Delta-reducing the reproducer
removes `copy` entirely — five lines suffice:

```python
class T:
    def __init__(self, args):
        self.args = args
tree = T(tuple([T(None)]))
print(tree.args[0].args)
```

Storing a list-layout tuple in a record field and reading it back is the
whole mechanism. Controls that all **pass**, which is what localises it:

| variant | field type | recursive | result |
|---|---|---|---|
| `T(tuple([inner]))`, `inner = T(0)` | `{int, tuple}` | yes | pass |
| `B(tuple([A()]))`, two classes | `{None, tuple}` | no | pass |
| `T([T(None)])` — list, not tuple | `{None, list}` | yes | pass |
| `T(tuple([T(None)]))` | `{None, tuple}` | yes | **fail** |

Probing `set_tuple_able` at clone time separates the passing and failing
cases exactly:

```
ms5 (passes)  [ta] cs=947 vars=0 elem=SET     able=0   -> LIST layout
ms8 (fails)   [ta] cs=947 vars=0 elem=bottom  able=1   -> RECORD layout, 0 members
```

An empty record. `cg.cc` then reports it in whichever disguise the
program reaches first — `_CG_prim_copy_dst(_CG_void, …)` through
`__deepcopy__`, or `assert(!"runtime error: bad getter")` through a
plain subscript. Same defect; the copy path was never involved.

### Why the element was bottom

Tracing the constraint per pass:

```
[makeseq/fv] fvout=bottom      pass 1
[makeseq/fv] fvout=bottom      pass 2
[makeseq/fv] fvout=SET         pass 3
[makeseq/fv] fvout=bottom      pass 4   <- LAST pass
```

`update_gen(elem, fv->out)` is a **snapshot**. Nothing orders the
`make_seq` constraint after the source literal's own `make` within a
pass, and nothing re-runs it when the source is repopulated after the
per-pass `clear_avar`. The type was computed and then lost.

`arg_of_send` does not rescue it: the source var does not *change*
during the final pass, so nothing re-enqueues the send.

### The fix

A durable **edge**, not a snapshot — via `vector_elems`, which already
exists for precisely this shape. Both the source's generic element and
its per-index vars are CS-contoured, and a raw CS → CS `flow_vars` puts
a CS-contoured var in `elem->backward`, which is what `compute_setters`
asserts against (`x->contour_is_entry_set` — genetic2_idioms aborts the
compiler; that assertion is why the original code reached for
`update_gen` in the first place). `vector_elems` lands each value in a
fresh entry-set-contoured tval of the pnode first, so every edge into
the element starts at an ES var.

### Standing state

    default settings   276 passed / 0 failed   (PYC_MAKESEQ gates the
                       only emission site, so the path is dead here)
    both flags on      274 passed / 3 failed

The three, and what each needs:

- **`builtin_type_factory`** — not a failure. pyc now prints `(1, 2, 3)`
  where the `.check` records the old buggy `[1, 2, 3]`. Regenerate the
  check when the flags default on.
- **`deepcopy_objects`** — the copy aliases the original (prints `77`
  where CPython prints `5`). `class tuple` has no `__deepcopy__` and
  falls through to `__pyc_any_type__`'s one-level struct clone.
- **`list_element_type_union`** — `tuple(self.state[20:32])` yields
  wrong values (`0 0 1` for `0 15 16`).

**Retracted:** an earlier draft of this list claimed an identity
`tuple.__deepcopy__` "regresses `minmax_3arg`: `return self` flows every
tuple contour into one return AVar". That is wrong, twice over. Identity
cannot fix aliasing in the first place, and `minmax_3arg` was never
regressing — its check embeds `__pyc__.py` line numbers, which shift
when any builtin file grows (issues/111).


## Both remaining failures resolved, and what they cost

`a7e5b45d` fixed `list_element_type_union`; the `deepcopy_objects` fix
is written and verified but is **coupled to the flag flip** (below).

### `list_element_type_union` — two defects, one symptom

Neither was in the copy path either.

**1. `cs->vars.n` is not an arity.** Two places read it as one, and a
make_seq CreationSet has no per-index vars at all, so both silently read
zero: `P_prim_len` folded `len()` to the **constant 0**, so
`tuple.__hash__`'s `for k in range(len(self))` never ran and every tuple
hashed to 0; and `tuple_able()` elected RECORD layout with zero members
— the same empty record as before. `CreationSet::no_static_arity` marks
it at the make_seq constraint. Unlike the element type this is a
property of *how the container was made*, so it is known at constraint
time and immune to the pass-ordering race that `50ebc8aa` had to fix.

**2. `tuple.__eq__`/`__lt__` are generated at the wrong length.**
`inject_tuple_compare` unrolls them to `max_arity`, computed by scanning
tuple **literals** (`PY_tuple` nodes). A `tuple(iterable)` has no literal
anywhere, so it contributes nothing — the unrolled body compared only
the first `max_arity` elements and returned `True` for everything past
it. rubik2's `id_() -> tuple(state[20:32])` deduped 15 distinct 12-tuples
to **3** in a set, silently, with no diagnostic. With no tuple literal in
the program at all `max_arity` is 0 and every same-length tuple compares
equal. Both methods now get a runtime tail past the unrolled prefix;
for a RECORD tuple `n` is constant `<= max_arity` so the guard folds to
`False` and the tail is dead — the same folding the existing `n >= k`
guards already rely on.

A library `tuple.__eq__` was tried first and is **wrong**: it is
ambiguous against the generated one and breaks record-tuple equality at
default settings (`(1, 2) == (1, 2)` aborts).

### `deepcopy_objects` — fixed, but coupled to the flag flip

Element-recursive, mirroring `list.__deepcopy__`, rebuilding with
`make_seq` because the arity is a runtime value:

```python
def __deepcopy__(self):
    r = []
    for k in range(len(self)):
      r.append(self[k].__deepcopy__())
    return __pyc_primitive__(__pyc_symbol__("make_seq"), tuple, r)
```

Verified: `deepcopy_objects` MATCHES under the flags. But it cannot land
while they are off. The library method puts a make_seq site in **every**
program, and at default settings a make_seq CS still takes the record
branch (`tuple_is_record = sym == sym_tuple && !tuple_as_list_enabled()`)
— an empty record again. Measured: 273 passed / 3 failed at default
(`deepcopy_list`, `deepcopy_objects`, `deepcopy_none_or_tuple_field`).

So this fix and the flag default are **one commit, not two**.

## Standing state

    default settings   276 passed / 0 failed
    both flags on      275 passed / 2 failed

The two are `builtin_type_factory` (whose `.check` records the old buggy
`[1, 2, 3]`; pyc now correctly prints `(1, 2, 3)`) and `deepcopy_objects`
(fix above). Both are resolved by the flip itself.

Corpus, freshly measured on a clean tree at this commit:

    default    66 compiled (34 with warnings), 11 failed of 77
    flags on   66 compiled (34 with warnings), 11 failed of 77
    per-program status: IDENTICAL, and identical to the pre-change
                        baseline

Caveat: the sweep measures **compile only**. Roughly 39% of corpus
programs that compile then crash at runtime, so this is evidence the
flags are not a compile-time regression — not that they are behaviour-
neutral.

## What remains before the flags can default on

1. Land `tuple.__deepcopy__` + `PYC_MAKESEQ=1` + `PYC_TUPLE_AS_LIST=1`
   together.
2. Regenerate `builtin_type_factory.py.check` (records `[1, 2, 3]`).
3. Re-take both baselines afterwards, and ideally an execution-level
   corpus comparison rather than compile-only.

## BLOCKER: make_seq only understands list sources

Measured `tuple(x)` against CPython across source kinds, at both
settings. The flip is **not ready** — it is a clear win for list
sources and a silent regression for every other iterable.

| `tuple(src)` | CPython | default | flags on |
|---|---|---|---|
| list var / literal | `(1, 2, 3)` | `[1, 2, 3]` | `(1, 2, 3)` ✓ |
| list slice | `(1, 2)` | `[1, 2]` | `(1, 2)` ✓ |
| nested tuples | `((1, 2), (3,))` | `[[1, 2], [3]]` | `((1, 2), (3,))` ✓ |
| empty list | `()` | `[]` | `()` ✓ |
| **string** | `('a','b','c')` | `['a','b','c']` | **`()`** ✗ |
| **range** | `(0, 1, 2)` | `[0, 1, 2]` | **compile fail** ✗ |
| **set** | 2 elements | 2 | **0** ✗ |
| **dict** | 2 keys | runtime abort | **0** ✗ |

Operations, same comparison:

| | CPython | default | flags on |
|---|---|---|---|
| `tuple(xs) == (1, 2)`, `hash`, `set` dedup | True/True/1 | **runtime abort** | True/True/1 ✓ |
| `<` / `>` between two dynamic tuples | True/True | **runtime abort** | True/True ✓ |
| dict key | 2 / first second | ✓ | ✓ |
| `tuple(a) + tuple(b)` | `(1,2,3,4)` | `[1,2,3,4]` | `[1,2,3,4]` ✗ |
| `t[0] = 99` | TypeError | mutates | mutates ✗ |

So the flip fixes two runtime aborts and makes every list-sourced tuple
exactly match CPython — and trades "right elements, wrong type" for
**silently empty** on strings, sets and dicts, plus a compile failure on
range. Trading a visibly-wrong type for a silently-wrong length is a
net loss; a `tuple(some_string)` that yields `()` is far more dangerous
than one that yields a list.

### Cause

The `P_prim_make_seq` constraint pulls element types out of the source
CreationSets' per-index `vars` and generic element. That describes a
**list**. A string is not a container CS with element vars; a set and a
dict keep their contents elsewhere; a range is a lazy iterator object.
None of them contribute anything, so the element stays bottom and the
runtime copy has nothing to copy.

The range case does not even get that far:

```
fail: mismatched field sizes: class 'closure' field '<anon>'
      mixes 8- and 0-byte members ('__pyc_tolist__')
```

— the iterator's method-pointer union, the ifa/issues/105 family.

### What it needs

make_seq has to go through the **iterator protocol** rather than
peeking at container internals: `__iter__`/`__next__`, the same path
`list(x)` already takes for these sources (`list("abc")` is correct at
default today, so the machinery exists). That is the gating work before
`PYC_MAKESEQ` and `PYC_TUPLE_AS_LIST` can default on.

`tuple.__add__` returning a list is a separate, now-easy follow-up: it
was a documented compromise only because fixed-arity structs cannot
concatenate at runtime, which make_seq removes.

## Attempt 1 at the iterator-protocol route — measured, reverted

The plan above ("make_seq has to go through the iterator protocol")
was tried and does **not** work as a lowering change alone. Recording
it so it is not tried again blind.

### What was built

Rather than reimplement iteration inside the FA constraint, feed
make_seq the list that `__pyc_tolist__` already produces — that method
is defined on str, bytes, list, tuple, range, set and dict, so
`tuple(x)` inherits `list(x)`'s whole iterable surface for free:

```
tmp = x.__pyc_tolist__()        # existing, works for every iterable
result = make_seq(tuple, tmp)   # list -> runtime-arity tuple
```

That immediately fixed the three broken sources — `tuple("abc")`,
`tuple(range(3))`, `tuple(a_set)` all became exact — and immediately
broke the list sources, because `list.__pyc_tolist__` copies through a
method boundary that FA loses the element type across:
`for x in tuple(xs)` summed **0** instead of 6.

Second iteration: add `__pyc_seq_source__`, defaulting to
`self.__pyc_tolist__()` on `__pyc_any_type__` and overridden to
identity on `class list` (make_seq copies its source anyway, so a list
needs no intermediate). That restored the list sources — iteration,
slicing and indexing all correct again — while keeping string, range
and set fixed.

### Why it was still reverted

The *added dispatch alone* destabilizes tuple element typing in shapes
that previously worked. Even with the identity override, three probes
that passed before now abort at runtime with "expression has no type"
on the tuple's element:

- `tuple(xs) == (1, 2)` — dynamic tuple vs literal tuple
- `tuple(xs) < tuple([1, 3])` — ordering between two dynamic tuples
- `tuple([t, tuple([3])])` — nested dynamic tuples

Net on the probe set: 7 ✓ / 4 ✗ either way — a wash, just a different
four. Net on the suite, which is what decided it:

    default    276 passed / 0 failed  (the 1 reported is minmax_3arg
                                       line drift, issues/111)
    flags on   274 passed / 4 failed  (committed state: 275 / 2)

Landing a change that makes the flags-on path *worse* to fix probes is
not a trade worth taking, so it was reverted. The tree is back to
276 / 0 and 275 / 2.

### Root cause: the call's return is EMPTY on the final pass

Traced, and it is sharper than "element types do not survive a call
boundary" — plain call boundaries are fine. On the committed tree all
of these are correct:

```python
def mk(xs): return tuple(xs)        # make_seq inside a function
def src(): return [1, 2, 3]         # source is a returned value
a = mk([1, 2, 3]); tuple(src())     # both exact, incl. iteration and ==
```

The failing shape needs only three lines, and only under the rework:

```python
a = tuple([1, 2])
b = tuple([1, 3])
print(a < b)        # runtime abort: matching function not found
```

Probing the make_seq constraint per pass (`src->out->sorted.n`, the
CreationSets of the value handed to make_seq):

```
[ms] cs=945 srcCSn=0 srcvars=-1 elem=bottom     pass 1
[ms] cs=948 srcCSn=0 srcvars=-1 elem=bottom
[ms] cs=945 srcCSn=0 srcvars=-1 elem=bottom     pass 2
[ms] cs=948 srcCSn=0 srcvars=-1 elem=bottom
[ms] cs=945 srcCSn=1 srcvars=2  elem=SET        pass 3  <- correct
[ms] cs=948 srcCSn=1 srcvars=2  elem=SET
[ms] cs=945 srcCSn=0 srcvars=-1 elem=bottom     pass 4  <- LAST
[ms] cs=948 srcCSn=0 srcvars=-1 elem=bottom
```

The source AVar — the return value of `list::__pyc_seq_source__`, an
identity method — has **zero CreationSets** on the final pass, having
been correct on pass 3. Not a union, not a wrong type: empty. So the
constraint creates no edges at all that pass, the tuple element ends
bottom, and the generated `__lt__`'s runtime tail (`self[i] < t[i]`,
a non-constant index that reads the generic element) has nothing to
dispatch on.

Confirmed it really is the identity method and not a fallback: only
`list::__pyc_seq_source__` is emitted, `__pyc_tolist__` is never called.

This is the same **per-pass rebuild** hazard as `50ebc8aa`, one level
up. There the *element* was a snapshot; here the *CreationSet set* of
the source is, and the constraint's durable edges are themselves
rebuilt each pass — so a single pass where the source reads empty
discards them. Adding one call in the hot path is enough to land the
last pass mid-churn (see the sticky stall guard in `extend_analysis`,
which can stop the outer loop on such a pass).

### Consequence for the design

Do **not** reach for the iterator protocol via a method call. Two
routes that avoid adding an FA-visible call:

1. **Special-case the source kind in the constraint**, and emit the
   conversion in **codegen** rather than as a Python-level call: for a
   string source the element is `str` and cg emits a character loop;
   for bytes, `int`. Sets and dicts already have element AVars for FA
   to read, and each needs its own emitted loop. Contained, no new
   call boundary, but real work per source kind.
2. **Make the constraint's edges survive a pass where the source reads
   empty** — e.g. remember the resolved source CSs on the CreationSet
   across passes, the way `split_origin` and `elem_key` are durable.
   This is the general fix and would likely help well beyond make_seq,
   but it is FA-core surgery.

Route 2 is the principled one and subsumes route 1.


## Route 2 landed, and it rescued route 1 (`f6d6d46a`)

The two designs above were not alternatives — the second is a
prerequisite for the first.

`CreationSet::seq_src` remembers the last **non-empty** set of source
CreationSets make_seq saw, and the constraint drives its element loop
from that rather than from `src->out` directly. Alone it is inert (both
suites unchanged). With it in place, the `__pyc_seq_source__` lowering
that had been reverted twice now works:

| `tuple(src)` | CPython | default | flags on |
|---|---|---|---|
| list var / literal / slice | exact | wrong type | ✓ |
| nested tuples | `((1, 2), (3,))` | `[[1, 2], [3]]` | ✓ |
| empty | `()` | `[]` | ✓ |
| **string** | `('a','b','c')` | `['a','b','c']` | ✓ **fixed** |
| **range** | `(0, 1, 2)` | `[0, 1, 2]` | ✓ **fixed** |
| **set** | 2 elements | 2 | ✓ **fixed** |
| **dict** | 2 keys | runtime abort | runtime abort |
| `==` vs literal, `hash`, `set` dedup | | **abort** | ✓ |
| `<` / `>` between dynamic tuples | | **abort** | ✓ |

**10 of 11 probes exact**, up from 7. Measured:

    default settings   276 passed / 0 failed   (unchanged)
    both flags on      275 passed / 2 failed   (unchanged count)
    corpus             66 compiled / 11 failed of 77, per-program
                       IDENTICAL to the pre-change baseline at BOTH
                       settings

The flags-on count is unchanged because its two remaining failures are
`builtin_type_factory` (stale `.check`) and `deepcopy_objects` (needs
the flip) — the suite does not yet cover the iterable surface that
actually improved.

### What is left before the flags can default on

1. ~~**`tuple(a_dict)`**~~ — DONE (`2bb302b0`). Neither of
   `07_dict.py`'s two `__pyc_tolist__` definitions was on `dict`: both
   are on the ITERATOR classes, so `list(d.keys())` worked and
   `list(d)` aborted with "getter not resolved" — a default-settings
   bug, not a flag-specific one. `dict.__pyc_tolist__` added, reading
   `_keys` directly rather than through `self.keys()`, since the extra
   call boundary is what costs make_seq its source CreationSets on a
   churning final pass. **The probe set is now 11 of 11 exact.**
   Covered by `tests/dict_iterable_conversion.py`.
2. **Land the flip** together with `tuple.__deepcopy__` and a
   regenerated `builtin_type_factory.py.check`, per the earlier
   section — those cannot land separately.
3. **`tuple(a) + tuple(b)`** still returns a list, and `t[0] = 99`
   still mutates where CPython raises `TypeError`. Both are
   pre-existing and independent of the flip; `__add__` is now easy
   with make_seq available.


## Iterable surface: complete

    tuple(src)                    default        flags on
    list / slice / nested / empty wrong type     exact
    string                        wrong type     exact
    range                         wrong type     exact
    set                           wrong type     exact
    dict                          wrong type     exact
    == vs literal, hash, dedup    RUNTIME ABORT  exact
    < / > between dynamic tuples  RUNTIME ABORT  exact

11 of 11 probes exact against CPython under the flags. Every remaining
difference at default settings is the wrong *type* (a list), which is
the whole point of the flip.

Suites, at `2bb302b0`:

    C backend           277 passed / 0 failed
    LLVM backend        279 passed / 1 failed
    C, both flags on    276 passed / 2 failed
    corpus              66 compiled / 11 failed of 77, per-program
                        identical to baseline at both settings

**Note on the LLVM row**, which earlier entries in this issue did not
account for: `make test-e2e` runs `test_pyc.py` TWICE -- once for the C
backend and once with `PYC_FLAGS=-b` -- and prints two summaries. The
LLVM failure is `list_element_type_union`, pre-existing (verified by
stashing) and documented in that test's own header as the open
ifa/issues/051 bug. It is not tagged, so it reports as a plain failure
in the LLVM phase.

Remaining before the flip: only items 2 and 3 above -- land
`tuple.__deepcopy__` + the regenerated `builtin_type_factory.py.check`
together with the defaults, and (independently) `tuple.__add__`
returning a list and `t[0] = 99` not raising.


## The flip was attempted and is HELD on issues/112

Flipping both defaults, adding the element-recursive
`tuple.__deepcopy__`, and regenerating the two stale checks produced
three C-backend failures, of which two were the expected regenerations
(`builtin_type_factory`, whose check records the old buggy
`[1, 2, 3]`, and `minmax_3arg`, pure `__pyc__.py` line drift). The
third is real and new:

**`deepcopy_objects` aborts.** Delta-reduced to 18 lines
(`tests/deepcopy_tuple_copy_of_copy.py`): deep-copying a tuple TWICE
leaves `self[k]` unresolved inside `tuple.__deepcopy__`. One copy is
fine. The identical program with a list instead of a tuple passes.

`tuple.__deepcopy__` can be neither omitted (the any-type fallback is
a one-level struct clone, so the copy shares elements and the test
silently prints 77 for 5) nor included (it aborts) — and the test
**passes today**, because `tuple(iterable)` still returns a list and
lists deep-copy correctly. So the flip currently trades correct
deepcopy behaviour for silent aliasing or a crash, which is the same
bar that rejected the earlier attempt above.

Filed as **issues/112**, with the three things measurement has already
ruled out (element loss, the make_seq rebuild, the pass-1 no-clone
path).

**`tuple.__add__` returning a list is blocked by the same thing** — it
needs `make_seq` in the library, which is only sound once the layout
flag defaults on. It is not an independent follow-up after all.

State restored: C backend 277 / 0, LLVM 279 / 1 (pre-existing
ifa/051), both flag defaults still off.
