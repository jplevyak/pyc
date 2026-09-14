# 128 — a union receiver promotes every class's fields onto every other class

**Status:** open, root-caused as far as the evidence goes; one gap named
below. Filed 2026-09-14 while asking why `chull`'s `{Vertex, Edge}` union
forms.

**Numbering note:** `issues/` and `ifa/issues/` are separate trees; this is
`issues/128`, unrelated to `ifa/issues/128`.

## The defect

`P_prim_setter` (`fa.cc:3616`) handles `obj.field = val` by walking **every
CreationSet in the receiver's type** and, for each that lacks the field,
recording it for promotion:

```c
for (CreationSet *cs : obj->out->sorted) {
  AVar *iv = cs->var_map.get(symbol);
  if (iv) flow_vars(tval, iv);
  else    cs->unknown_vars.add(symbol);      // <- promote it onto this class
}
```

So when the receiver is a UNION, every class in it acquires every other's
fields. `promote_field` appends to `cs->sym->has`, and that index **is** the
emitted struct's `eN` suffix — so the same field name lands at whatever slot
each class had already reached.

On `shedskin_examples/chull`:

```
Edge:    adjface 15  delete 16  endpts 17  enum 18  mark 19  newface 20
         duplicate 21  onhull 22  visible 23
Vertex:  duplicate 16  mark 17  onhull 18  v 19  vnum 20
         delete 21  newface 22  visible 23
```

`Edge` receives Vertex's `duplicate`/`mark`/`onhull`, `Vertex` receives
Edge's `delete`/`newface`, both receive Face's `visible` — and **slot 22 is
`onhull` on `Edge` and `newface` on `Vertex`**. A later read through a union
receiver blind-casts across the two layouts, which
[ifa/123](../ifa/issues/123-CGEN-union-receiver-field-access-has-no-discrimination.md)'s
contract catches as

```
error: object layout: 'Edge' is blind-cast to 'Vertex' and read at e23,
       but member width differs at e22 (_CG_bool vs _CG_void)
```

The layouts are **byte-identical at the default and under
`PYC_CSDCPA1=2`**, so this is pre-existing and latent on main — which is
why `chull` compiles there and then SEGFAULTS (`run 139`). The start-merged
flip only makes a read go through the union, so the check fires at compile
time instead of memory being corrupted at runtime.

## Reproducer — 14 lines, no inheritance

```python
class A:
    def __init__(self):
        self.a = 1
class B:
    def __init__(self):
        self.b = 2
xs = [A(), A()]
ys = [B(), B()]
xs[0].a = 5
ys[0].b = 6
print(xs[0].a)
print(ys[0].b)
```

`IFA_DBG_LAYOUT` gives `A.a 14  A.b 15  B.a 14  B.b 15` — both classes get
both fields. It still RUNS correctly here only because the two indices
happen to agree; in `chull` the classes have different pre-promotion field
counts (15 vs 16) so they do not.

## The evidence is transient

`IFA_DBG_PROMOTE=1` (added with this issue) names each write whose receiver
is a union. On the reproducer **and on `chull`, every one of the sites is at
pass 0** — 4 of 4, and 777 of 777 respectively. The union is gone by pass 1;
`clear_results()` clears `unknown_vars` before each pass, so the derived
state is reset, but `promote_field` has already written into
`cs->sym->has`, which is per-CLASS and never revisited.

**So the decision outlives the evidence for it.** That is the same shape
this tree keeps meeting from other directions —
[ifa/128](../ifa/issues/128-cs-identity-over-discriminates-vs-element-type.md)'s
"what persists is the DECISION, not the types".

## What is NOT the cause — each ruled out by measurement

- **Not a shared container method contour** ([ifa/143](../ifa/issues/143-shared-container-method-contours-refuse-cs-splits.md)).
  `IFA_DBG_FUNES=__getitem__` shows **2 contours**, one per list
  (`list#1004`, `list#1005`). The method splits correctly.
- **Not a merged container** ([ifa/133](../ifa/issues/133-split-a-container-on-its-element-type.md)).
  `IFA_DBG_ELEMTYPE` on `chull`: `list: 40 CS / 12 elemtypes`, `mixed=0`.
  The lists are separated.
- **Not the start-merged flip.** Layouts byte-identical at both arms.
- **Not `ifa/135`.** That issue claims `chull`; `Edge` and `Vertex` are
  unrelated classes, so neither its prototype route nor its empty-sibling
  clone merge applies. Corrected there.
- **Not slot ordering alone** ([121](closed/121-sibling-subclass-field-layout.md)).
  Name-sorting each promotion batch cannot align classes whose
  pre-promotion `has` counts already differ.

## Step 1 DONE — there is no second path, and the blunt rule is wrong

**Retracted:** this issue previously said a second path reaches
`unknown_vars` that `P_prim_setter` does not cover. **There is none.** That
claim came from an experiment gated on `analysis_pass == 0`, and the union
writes that matter are not all at pass 0. Suppressing them at EVERY pass
accounts for all of it.

Established with two probes — `IFA_DBG_PROMOTE` (writes whose receiver is a
union) and `IFA_DBG_PROMOTED` (the promotions that result). On `chull` the
sets line up exactly:

```
promoted onto Edge:    adjface delete duplicate endpts enum mark newface onhull visible
promoted onto Vertex:  delete duplicate mark newface onhull v vnum visible
union-receiver writes: delete duplicate mark newface onhull visible
```

Every CROSS-class field is in the union-write set; every own field
(`adjface`/`endpts`/`enum`, `v`/`vnum`) is not. One path, fully accounted.

And suppressing it works on `chull`: **rc=0, zero errors**, with each class
getting exactly its own five fields —

```
Edge:   adjface delete endpts enum newface
Vertex: duplicate mark onhull v vnum
```

— byte-for-byte the field sets shedskin emits. The pyc suite stays at
**316/0**.

### But "never discover through a union" is the WRONG RULE

Corpus, flip default, union-discovery suppressed
(`compile__PYC_PROMOTELATE_1__5dbfb6f2+3488e405`) against the flip alone:

| | flip | + suppressed |
| --- | --- | --- |
| compile failures | 7 | **8** |
| programs with warnings | 33 | **29** |

`chull` is fixed and **two programs are lost**:

- **`richards` rc=1** in 3s — a genuine compile failure,
  `no matching function for call`. A field that only a union-receiver write
  attests was dropped, so a value's type is wrong downstream. This is the
  rule being too blunt, not a latent bug being exposed.
- **`sunfish` rc=124** — a TIMEOUT at 400s, i.e. an analysis-cost blowup
  rather than a correctness failure. Worth understanding separately.

**So the lever is removed rather than kept**, and this is the useful result:
the distinction that matters is not *union vs single* receiver, it is
*transient vs persistent* evidence. A union write at pass 0 that is gone by
the fixed point should leave nothing behind; a union write that survives to
the fixed point is the only evidence the field exists and must be kept.
Suppression cannot tell them apart. **Recomputing the promoted part of
`has` every pass can** — which is the design's step 3, and this measurement
is the argument for it over the cheaper rule.

## How shedskin does field discovery — read from source, verified on both shapes

`obj.attr = val` is lowered the same way pyc lowers it: to a fake
`obj.__setattr__("attr", val)` call (`graph.py:assign_pair`). The difference
is what happens next.

**`connect_getsetattr` (`infer.py:1350`) attaches the field to the class the
call RESOLVED to**, not to every class in the receiver's type:

```python
parent = func.parent                       # the class this call dispatched to
assert isinstance(parent, (python.Class, python.StaticClass))
var = default_var(gx, varname, parent, worklist, mv=parent.module.mv)
```

`func` is the resolved target under CPA's per-receiver templates, so each
instantiation sees ONE class. pyc's `P_prim_setter` instead takes a single
receiver AVar and loops over **all** of `obj->out->sorted`, so one write
with a union receiver fans the field onto every member at once.

**But that is not the whole defence, and on its own it would not be one** —
if the union is real, CPA instantiates a template per class and each still
acquires the field. The primary defence is that **the union does not form**:

```cpp
list<A *> *xs;      // on this issue's 14-line reproducer
list<B *> *ys;
class A : public pyobj { __ss_int a; };
class B : public pyobj { __ss_int b; };
```

Each class gets exactly its own field, because `xs[0]` is precisely `A *`.
Same on `chull`: `Edge::endpts` is `list<Vertex *> *` and `adjface` is
`list<Face *> *`.

**And when the union IS genuine, shedskin says so.** Changing the reproducer
to `xs = [A(), B()]` with `x.a = 5`:

```
*WARNING* u4.py:9: expression has dynamic (sub)type: {A, B}
class A : public pyobj { __ss_int a; };
class B : public pyobj { __ss_int a;  __ss_int b; };
```

It promotes `a` onto `B` — the same thing pyc does — but it **warns**, and
the shared field lands at index 0 in BOTH classes, so a cast between them
reads `a` correctly. pyc does neither: it is silent, and the shared field
lands at whatever slot each class had reached.

So the three things pyc is missing, in priority order:

1. **Element precision, so the union does not arise.** This is what
   shedskin actually relies on and what `chull` needs.
2. **A diagnostic when a field is promoted across a union.** Today this is
   silent, which is why `chull` reached a runtime segfault on main with a
   clean compile. shedskin's "dynamic (sub)type" warning is the model.
3. **A consistent offset for a shared promoted field**, if it is promoted
   at all. Whether shedskin's agreement here is by construction or by
   insertion-order luck was NOT established and should be checked before
   copying it.

## How shedskin avoids it

Verified by translating `chull` with shedskin (rc=0) and reading the output:
each class carries exactly its own fields, no cross-class promotion
anywhere. It gets there two ways — the union never forms (`Edge::endpts` is
`list<Vertex *> *`, `adjface` is `list<Face *> *`, and even the all-`None`
default argument is a distinct `list<void *> *`), and where a union
legitimately does form it **hoists** the member to the lowest common
ancestor as a virtual method or `virtualvars` (`shedskin/virtual.py:125`),
so C++ inheritance gives one slot at one offset. Detail in
[ifa/135](../ifa/issues/135-empty-sibling-contour-wins-the-clone-merge.md).

## The design (author, 2026-09-14)

> *"Seems like we should recompute `sym->has` every pass. Adding to
> `sym->has` should be a front end configuration since that is not something
> all languages need."*

Both halves check out against the code, and together they are the fix.

### Where the boundary actually sits today — verified

**Adding to `Sym::has` during analysis is ALREADY frontend-only.** Every
mutation is in `python_ifa_sym.cc:424` (`promote_field`) or
`python_ifa_build_syms.cc` (class construction). `ifa/analysis/fa.cc` never
adds. The only other writers are `clone.cc:858/908/1453`, which REBUILD
`has` for clones after convergence, and the test IR builder. So half of the
second directive is satisfied.

**What is NOT frontend is the evidence collection.**
`CreationSet::unknown_vars` is declared in `ifa/analysis/fa.h:309` and
populated by generic ifa in `P_prim_setter` (`fa.cc:3633`):

```c
for (CreationSet *cs : obj->out->sorted) {
  AVar *iv = cs->var_map.get(symbol);
  if (iv) flow_vars(tval, iv);
  else    cs->unknown_vars.add(symbol);
}
```

That encodes *"a write to a field the class does not have DISCOVERS a
field on it"* — a Python-ism. A language whose classes declare their fields
wants the `else` branch to be an ERROR, not a discovery. Generic ifa should
not be asserting it.

### And `unknown_vars` is already reset per pass — `has` is not

`clear_cs` (`fa.cc:7957`) clears `unknown_vars`, and `clear_results` calls
it for every CreationSet before each pass. So the *derived* state is already
handled correctly. `sym->has` is the thing that accumulates, and nothing
ever clears it. **That asymmetry is the bug**: the evidence is reset each
pass, the conclusion drawn from it is not.

It is also exactly why the measurements above look the way they do — all
4/4 and 777/777 promoting sites are at pass 0, yet the fields survive to
codegen.

### The two changes

1. **Gate the collection behind `IFACallbacks`.** A hook in the shape the
   file already uses for frontend language policy —
   `narrowing_is_none_name()`, `bool_is_numeric()`, each defaulting to
   "this frontend has none":

   ```c
   // ifa.h
   virtual bool discovers_fields_by_write() { return false; }
   ```

   With it false, `P_prim_setter`'s `else` branch stops recording and the
   missing field is a violation like any other. pyc returns true.

2. **Recompute the promoted part of `has` every pass.** Split `has` into
   the DECLARED entries (from the frontend's class definition, stable) and
   the PROMOTED ones (derived). Reset the promoted entries wherever
   `unknown_vars` is reset, and re-derive them from that pass's evidence.
   A transient pass-0 union then leaves nothing behind, which is precisely
   this issue's defect.

   `Sym` has bitfield space (`sym.h:66-83`) for an `is_promoted_field : 1`
   marker, so the partition is representable without a new structure.

### What to check before building it

- **`has` INDEX is the emitted struct's `eN` suffix.** Codegen runs after
  convergence, so only the final `has` matters for emission — but anything
  that caches an index MID-analysis would break. Find those first.
- **`clone.cc` rebuilds `has` for clones** (858/908/1453). Its interaction
  with a per-pass reset has to be worked out; clones are made after
  convergence, so it is probably fine, but "probably" is not measured.
- **This does NOT fix the genuine-union case.** With `xs = [A(), B()]` the
  union survives to the fixed point, and re-deriving each pass re-derives
  the same promotion. That case needs the other two things shedskin has: a
  DIAGNOSTIC (it warns `expression has dynamic (sub)type: {A, B}`; pyc is
  silent) and a consistent offset for the shared field. Keep them separate.

## The rule, measured (author, 2026-09-14)

> *"I think we need to do discovery for unions but clear each pass and use
> the union discovery as demand to split the union."*

All three parts, and the measurement says the third one has a carve-out that
matters. `IFA_DBG_FIELDSPLIT` classifies every union-receiver write by how
many members already have the field:

| | ALL-HAVE | MIXED | ALL-MISS |
| --- | --- | --- | --- |
| `chull` | 2540 | **162** | **0** |
| `richards` | 641 | **43** | **24** |

**ALL-HAVE** — every member has the field. The `iv` branch is taken, nothing
is recorded, nothing to do. It is the overwhelming majority.

**MIXED — this is the demand.** On `chull` every one of them is
`have=1 miss=9`: exactly ONE class has the field and nine do not, for
precisely the cross-class names (`delete`, `duplicate`, `newface`,
`onhull`, `visible`, `mark`). Something observed a distinction — *this class
has the field, those do not* — and could not proceed without inventing it
on nine classes. That is CLAUDE.md's definition of a demand, and the
partition it asks for is **{have} vs {miss}: exactly 2, named by the demand
itself**, never a count of things. It passes
[ifa/146](../ifa/issues/146-remove-all-arbitrary-splitting.md)'s test on
both questions — it cannot fire without the demand, and the demand alone
decides whether while field-presence decides which.

**ALL-MISS — not separable, and the carve-out that saves `richards`.**
`have=0 miss=3` for `handle`, `ident`, `input`, `link`, `packet_pending`,
`priority`, `task_holding`, `task_waiting`. No member has the field, so
partitioning by presence yields ONE group and there is nothing to split.
Here the write genuinely IS the only evidence the field exists, and it must
still be promoted. **This is exactly what blanket suppression destroyed** —
`richards` has 24 of them and broke; `chull` has none, which is why
suppression happened to fix it.

### So the rule is three-way

| receiver | action |
| --- | --- |
| ALL-HAVE | flow normally |
| MIXED | **demand: split the union {have} vs {miss}. Do NOT promote.** |
| ALL-MISS | promote — but as DERIVED state, re-derived every pass |

The per-pass clear is what makes the third row safe: a transient union at
pass 0 records a field, and if the union is gone at the fixed point the
field is not re-derived and leaves nothing behind. Persistent evidence
survives; transient evidence does not. That is the distinction blanket
suppression could not draw.

### Why this is the same shape as 146 E

Separating a receiver because a demand cannot proceed on the union, with the
partition named by the demand rather than by a caller or member count, is
what [146](../ifa/issues/146-remove-all-arbitrary-splitting.md) E already
owes for dispatch. This is the same mechanism with field-presence as the
predicate instead of dispatch resolution, and it is bounded at 2 by
construction where E's is bounded by the receiver's classes. Whoever builds
one should look at the other.

## Built: the demand is RECORDED, and it is not actionable at the receiver

**Author's correction, 2026-09-14:** *"keep the mixed write fields but have
them demand the split. Richards has a real union."* Right on both counts,
and measured:

- **Dropping a MIXED write is never sound.** Tried it — `chull` compiles
  clean with each class getting exactly its own five fields (matching
  shedskin), and `richards` STILL fails with `no matching function for
  call`. So a MIXED write can be the legitimate first write of a field onto
  a class that really has it. Reverted.
- **The demand is additive instead.** MIXED now promotes exactly as before
  AND records the receiver in `fieldsplit_demands`, cleared per pass beside
  `tc_cs_dropped` so an imprecise early pass leaves no standing demand.
  `richards` stays green, which is the point of making it additive.

`PYC_FIELDSPLIT=1` (default 0) drains the list before the CreationSet last
rung, on quiescence.

### The result: 162 demands, 0 splits

On `chull`: **162 MIXED writes recorded, and `split_entry_set` fired zero
times.** Every one is rejected by its preconditions — an ES-contoured
**formal**. chull's receivers are loop locals (`for e in self.edges:` then
`e.newface = None`), and you cannot filter an edge by a local's value. It is
the same wall stage 1 already names with `tc_skip_rval`
("ES/non-formal-rval skipped").

**So the demand is real and correctly identified, and the receiver is the
wrong place to act on it.** The split has to happen at whatever contour
FEEDS the local — which is exactly
[ifa/133](../ifa/issues/133-split-a-container-on-its-element-type.md)'s
`PYC_ESBLOCK` shape ("find the blocker BY TEST": walk back to the formal of
a contour with more than one in-edge, hold it terminal, and check whether
the groups then separate) and
[146](../ifa/issues/146-remove-all-arbitrary-splitting.md) E's.

Two things follow, and they are why this is left gated rather than pushed
further here:

1. **This wants ESBLOCK's blocker-finding, not a second copy of it.** The
   next step is to feed `fieldsplit_demands` into that walk instead of
   calling `split_entry_set` on the receiver directly.
2. **The partition must stay at 2.** The current action splits by TYPE,
   which on `chull`'s 10-class union could hand back up to 10 groups —
   ifa/144's fan signature. The demand names exactly
   `{have}` vs `{miss}`; whatever acts on it must group edges by
   field-presence, not by type. `decide_entry_set_split`'s stay/do
   structure is the place to add that key.

Default arm unchanged (`chull` still rc=1 at the default, suite 316/0), so
this is scaffolding with a measured population, not a behaviour change.

## Plan

Ordered so each step is measurable on its own.

1. ~~**Find the second path into `unknown_vars`.**~~ **DONE — there is
   none.** See above. The single `P_prim_setter` site accounts for every
   cross-class promotion on `chull`, and suppressing it fixes `chull` with
   the suite still at 316/0 — but costs `richards` (a real failure) and
   `sunfish` (a timeout), which is why the blunt rule is not the fix.

2. **Build the three-way rule above**, in this order: the `IFA_DBG_FIELDSPLIT`
   classification already exists, so start by making MIXED a demand that
   splits the receiver, leaving ALL-MISS promoting exactly as today. That
   alone should fix `chull` without touching `richards` — the prediction is
   explicit and falsifiable, since `chull` is 0 ALL-MISS and `richards`'
   failures were all ALL-MISS.

3. **Gate field-discovery-by-write behind `IFACallbacks`**
   (`discovers_fields_by_write()`, default false). Mechanical, no behaviour
   change for pyc, and it puts the language assumption where the other
   frontend policies already live. Doing it first makes step 3's blast
   radius visible: every site that would break with the hook off is a site
   that assumes Python semantics.

4. **Recompute the promoted part of `has` every pass.** Mark promoted
   entries, reset them where `unknown_vars` is reset, re-derive. This is the
   fix for THIS issue's defect — a transient union leaving a permanent
   field. Verify on the 14-line reproducer (`A` keeps only `a`) and then on
   `chull`, and check the `has`-index and `clone.cc` questions above.

5. **Then the genuine-union case, separately.** A union of unrelated classes
   read through one receiver has no sound blind cast, and step 3 does not
   touch it. shedskin does two things pyc does not: it WARNS
   (`expression has dynamic (sub)type: {A, B}`) and it keeps the shared
   field at a consistent offset. The warning is cheap and is the higher
   value of the two — `chull` reached a runtime segfault on main with a
   clean compile, and a diagnostic there would have surfaced this years
   earlier.

   Do **not** reach for a better sort order: that makes a shared layout by
   coincidence and cannot work when the base offsets differ, which is
   exactly `chull` (15 vs 16). The sound options are shedskin's — hoist to
   a shared base, a REPRESENTATION property and legitimate ground — or
   refuse.

## What this unblocks

`chull` under the start-merged flip ([ifa/129](../ifa/issues/129-plan-demand-driven-creation-set-splitting.md)
step 2), and `chull`'s pre-existing runtime segfault on main, which is the
same defect going undiagnosed.
