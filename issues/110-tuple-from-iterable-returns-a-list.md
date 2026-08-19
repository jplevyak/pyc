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
[052](052-llvm-nil-test-on-scalar-union-prints-none-for-zero.md) turns on:
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