# Allocation Primitive Investigation

## TL;DR

There is no `__new` primitive to wire up. The three primitives needed
for class-instance testing — `new`, `.` (period/getter), and `.=`
(setter) — are **already registered** by `Primitives::Primitives(IF1*)`
which calls `prim_init(this, if1)` in `ifa/if1/prim_data.cc`. Their
transfer functions are **already implemented** in `ifa/analysis/fa.cc`
as cases in the per-primitive switch (`P_prim_new` at line 1814,
`P_prim_period` at 1719, `P_prim_setter` at 1763).

**The test harness needs no new C++ code.** The work is purely
fixture-side: write `.ir` files that use the existing
`(send @primitive @new …)` / `(send @operator obj @period field …)`
forms.

Two experiments proved this end-to-end:

1. `(send @primitive @new %Point => %obj)` — rc=0, `creates=1` on the
   top EntrySet (the allocation is tracked), 1 extra CS appears in
   `fa->css`.
2. `(send @primitive @new %Point => %obj)` followed by
   `(send @operator %obj @setter #px %k => %r1)` and
   `(send @operator %obj @period #px => %r2)` — rc=0, `creates=1`,
   13 CSes (because the per-field AVars + their type CSes flow
   through). Setter and getter both resolve.

## Investigation record

### Where the wiring lives

`Primitives` is constructed once during `new IF1` (the `IF1` ctor
allocates a `Primitives` and `ifa_init` is what kicks the chain).
`prim_init` populates `prim_map[nargs][slot]` with `Prim *` objects
indexed by the selector name. The relevant entries:

| Prim | name | slot | dispatched on SEND form |
|---|---|---|---|
| `prim_new` | `"new"` | `prim_map[0][0]` | `(send @primitive @new <ClassMeta> => obj)` |
| `prim_period` | `"."` | `prim_map[1][1]` | `(send @operator obj @period #field => result)` |
| `prim_setter` | `".="` | `prim_map[2][1]` | `(send @operator obj @setter #field val => result)` |

`Primitives::find(c)` walks `c->rvals[0]` (the selector kind) and
either `rvals[1]` (for `sym_primitive`) or `rvals[2]` (for
`sym_operator`) to pick the `Prim`. This happens during
`if1_finalize_bind_prims` (which `fa_setup_environment` calls
unconditionally).

### What the transfer functions do

* **`P_prim_new`** (`fa.cc:1814`):
  ```cpp
  AVar *thing = make_AVar(p->rvals[p->rvals.n - 1], es);
  for (CreationSet *cs : thing->out->sorted)
    creation_point(result, cs->sym->meta_type);
  ```
  Takes the last rval (the class sym used as a value → its abstract
  type is its meta-type's CS), walks each meta-CS, and creates a
  fresh CS for `cs->sym->meta_type` (which recovers the original
  class). The new CS lives in `result`'s contour, so
  `creates` on the calling EntrySet gets +1.

* **`P_prim_period`** (`fa.cc:1719`) — for each CS in `obj->out` and
  selector CS, looks up `cs->var_map.get(symbol_name)` and flows the
  field's AVar to `result`. If the selector doesn't match a field
  but matches a visible method, dispatch into that method.

* **`P_prim_setter`** (`fa.cc:1763`) — analogous: looks up the field
  AVar and flows the value's type into it. Records unknown fields in
  `cs->unknown_vars` for diagnostic.

### Symbol availability

All three selector Syms are registered by `init_builtin_symbols`
(called from `init_ast`, which `ifa_init` calls):

```cpp
new_builtin_symbol(sym_primitive, "__primitive", "primitive");
new_builtin_symbol(sym_period,    ".",           "period");
new_builtin_symbol(sym_setter,    ".=",          "setter");
new_builtin_symbol(sym_operator,  "__operator",  "operator");
new_builtin_symbol(sym_new,       "new");
```

`if1_get_builtin(if1, "new")` etc. all resolve. The `.ir` parser's
`parse_ref` for `@name` does exactly that lookup, so `@new`,
`@primitive`, `@operator`, `@period`, `@setter` are all spellable
from `.ir` today with no parser changes.

### Why the earlier attempt looked broken

The earlier `10_class_instance.ir` smoke test used `@new_object` (the
global variable, registered by `new_builtin_global_variable`) rather
than `@new` (the symbol that drives `prim_new`). `new_object` is just
a placeholder for pyc's `new(a:anytype)` function body — without
pyc's prelude.v loaded, it's an untyped global, hence the
`"new_object" has no type` error.

The actual allocation primitive is `@new` (sym_new), and using it
directly produces a class CS without any frontend setup.

## Plan

Three new fixtures + one update to the existing `10_class_instance`:

### Fixture: `11_alloc.ir`

Minimum: allocate a Point. Verify `creates=1`, rc=0.

```text
(type %Point :kind RECORD :has (%px %py))
(sym %px :in %Point)
(sym %py :in %Point)

(sym %obj :is-local)
(sym %tcont)

(fun %top
  :rets (%obj)
  :cont %tcont
  :body
    (send @primitive @new %Point => %obj))

(entry %top)
```

Expected golden: `rc=0`, `creates: 1`, `creation-sets` count one
higher than the no-class baseline.

### Fixture: `12_field_access.ir`

Setter then getter on the same field. Verifies that the field-AVar
flow works.

```text
(type %Point :kind RECORD :has (%px %py))
(sym %px :in %Point)
(sym %py :in %Point)

(sym %k :is-constant :immediate (int32 42))

(sym %obj :is-local)
(sym %r1 :is-local)
(sym %r2 :is-local)
(sym %tcont)

(fun %top
  :rets (%r2)
  :cont %tcont
  :body
    (send @primitive @new %Point => %obj)
    (send @operator %obj @setter #px %k => %r1)
    (send @operator %obj @period #px => %r2))

(entry %top)
```

Expected golden: `rc=0`, `creates: 1` (the Point allocation), more
CSes than 11 because the int32 constant flows into the field and
back out.

### Fixture: `13_setter_split.ir`

The real prize. Two allocation sites that write different types into
the same field, then a function that reads the field. The
setter-based splitter (stage 3) should specialize the reader per
allocation site.

```text
(type %Point :kind RECORD :has (%px))
(sym %px :in %Point)

(sym %k_i :is-constant :immediate (int32 1))
(sym %k_f :is-constant :immediate (float64 2.5))

;; Reader: takes a Point, returns px.
(sym %p :is-local)
(sym %r_read :is-local)
(sym %rcont)
(fun %read_x
  :args (%read_x %p)
  :rets (%r_read)
  :cont %rcont
  :body
    (send @operator %p @period #px => %r_read))

;; Top: allocate two Points, set one's px to int32, the other's to
;; float64, then call read_x on each.
(sym %a :is-local)
(sym %b :is-local)
(sym %sa :is-local)
(sym %sb :is-local)
(sym %ra :is-local)
(sym %rb :is-local)
(sym %tcont)
(fun %top
  :rets (%ra)
  :cont %tcont
  :body
    (send @primitive @new %Point => %a)
    (send @operator %a @setter #px %k_i => %sa)
    (send @primitive @new %Point => %b)
    (send @operator %b @setter #px %k_f => %sb)
    (send %read_x %a => %ra)
    (send %read_x %b => %rb))

(entry %top)
```

Expected golden: `creates: 2` (two allocation sites), `entry-sets >= 4`
(@__main__ + at least two %read_x ESes from splitting on allocation
origin). This would be the first stage-3 (setter-based) splitter
coverage.

### Update: `10_class_instance.ir`

Either:
- (a) Remove it — it's superseded by 11/12/13.
- (b) Rewrite it as a comment-only "this is what's now possible"
  pointer to 11/12/13.

(a) is cleaner. The phase doc currently references it; the doc
update would point at the new fixtures.

## Open questions / things to verify when implementing

1. Does the `creates=` field on EntrySet actually count distinct
   allocation sites, or just CSes-of-Point reachable? If it counts
   reachable CSes, two allocations might still show `creates=1`
   if the splitter merges them at the AVar level.

2. Does the stage-3 setter splitter need explicit pattern_match-style
   "calling convention" args on `%read_x` (i.e., `%read_x` as its own
   first formal, à la `05_call.ir`)? Almost certainly yes — pattern
   match arity needs to line up the same way.

3. Does the splitter actually fire on a single field with two writers?
   Stage 3 splits on "setters" which are the AVars that wrote to a
   field. If two distinct setters wrote different types, the reader's
   load result has multiple possible types and the splitter should
   specialize the reader. Worth a verbose-trace run to confirm.

4. Should `%Point` be `(type %Point :kind RECORD :has (%px %py))`
   (using two fields like `12_field_access`) or one field (like the
   draft for 13)? Single-field is simpler for the golden and the
   number of CSes is more predictable.

## Recommendation

Implement 11, 12, 13 in that order. Each is a strict superset of the
prior so a regression in 11 immediately points at allocation; in 12 at
setter/getter; in 13 at the splitter. Drop 10. Total: ~3 new
fixtures, no C++ changes, no parser changes — just `.ir` files.

Estimated effort: 30 minutes for 11+12 (write fixture, rebless,
inspect golden). Maybe 1-2 hours for 13 because answering the open
questions might require verbose-trace runs and possibly a fixture
shape tweak.
