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

## The confluence, located (2026-09-14)

Author: *"We know for certain that the contours can be materialized because
shedskin does it. As always, find the confluence, create the demand and do
the splits to get those contours."* Found it, with `IFA_DBG_CSVARS`.

`Hull`'s three list fields, and one of them is contaminated:

```
var=vertices type= list#1800 list#1802      elem= Vertex          clean
var=faces    type= list#1801 list#1911      elem= Face Face Face  clean
var=edges    type= list#1848 list#1887      elem= Vertex Edge Edge Edge Edge
```

**`Hull.edges`' element channel holds a Vertex.** That is the confluence,
and everything downstream follows from it: the loop variable in
`for e in self.edges:` becomes `{Vertex, Edge, …}`, `e.newface = None` is
then a MIXED write, the MIXED write promotes Edge's fields onto Vertex and
vice versa, the promoted slots collide (`Edge.onhull` 22 against
`Vertex.newface` 22), and the union read blind-casts across them.

### The value path is ALREADY split; the receiver is not

`ELEMWRITER` on `cs=1848`:

```
es=680  __setitem__  type= Vertex#1191                          <- PURE Vertex
es=497  __setitem__  type= Edge#1896                            <- pure Edge
es=93   __setitem__  type= Edge#1276 Edge#1971 Edge#1972         <- pure Edge
es=586  __setitem__  type= Vertex Edge Edge Edge Edge            <- already mixed
es=91/473/585/679  append  type= Vertex Edge Edge Edge Edge
```

This is [ifa/133](../ifa/issues/133-split-a-container-on-its-element-type.md)'s
`MIXELEM` reading exactly: **`__setitem__` already has one contour per value
type** — `es=680` writes only Vertex, `es=497`/`es=93` write only Edge — and
they all land in ONE element channel because the RECEIVER CreationSet is
one. Nothing is left to split on the value side.

### And route 4 cannot act, because `defs=1`

```
cs=1848  defs=1  DEF av=4927  es=82   fun=__init__      <- Hull.__init__'s `self.edges = []`
cs=1887  defs=1  DEF av=12882 es=191  fun=CleanEdges
```

`CS_DEF_PARTITION` partitions a CreationSet's own creation points and
declines at one. So the CS side has nothing to partition — this is precisely
the residual family 133 names, and it is why five mechanisms have missed it.

### Which makes the split ES-side, and CLAUDE.md already states the rule

> *an EntrySet is split **so that** a CreationSet split becomes possible.*

`cs=1848` has one creation point because `Hull.__init__` has one contour. A
shared `__setitem__`/`append` contour serves that receiver alongside a
Vertex-holding list, so the Vertex reaches the edges element channel. Split
the contour that shares them and the SAME site is reached in two contours →
`creation_point` mints a second CreationSet → `defs=2` → route 4 partitions
→ two element channels, one Vertex, one Edge.

That is the chain shedskin gets for free by parameterising `list<T>`: it
emits `list<Vertex *>` and `list<Face *>` as distinct types. The contours are
materializable; pyc has to reach them by splitting.

## Step 1 DONE — the census, and a discriminator the plan did not anticipate

`IFA_DBG_ELEMCONF` reports every element channel holding two or more
distinct CLASSES, classified two ways.

**Are the writers separated?** SEPARABLE means two writers carry disjoint
class sets — on `chull`, `es=680 __setitem__ Vertex` against
`es=497 __setitem__ Edge`, so only the RECEIVER is shared and an ES split
has a key. FUSED means every writer already carries the whole union, so
there is nothing to split on.

**Do the classes share a USER-DEFINED ancestor?** This one was not in the
plan and it changes what the answer should be:

```
chull:    Vertex -> object __pyc_any_type__
          Edge   -> object __pyc_any_type__       shared: builtin roots only
richards: WorkTask -> Task __pyc_any_type__
          IdleTask -> Task __pyc_any_type__       shared: Task
```

Every pyc class specializes `object` and `__pyc_any_type__`, so "shares an
ancestor" is trivially true; the informative test is whether the shared
ancestor is **user code** (`!is_builtin`) — structural, no names. Two tries
failed first: `implementors.n < class count` passes for `object` too, since
its implementors exclude builtins.

- **UNRELATED** (`chull`) — a union of classes with nothing in common. A
  precision failure, and what should be split away.
- **RELATED** (`richards`) — `DeviceTask`/`HandlerTask`/`IdleTask`/`WorkTask`
  all derive from `Task`. **Legitimate polymorphism that must NOT be split.**
  This is why dropping the write broke `richards`, and it is exactly the case
  shedskin answers by HOISTING the field to the common ancestor
  (`virtual.py`'s `virtualvars`), not by separating.

### The corpus population

```
confluences=87   separable=25 (related=11  UNRELATED=14)   fused=62
```

across 14 programs. The **stop condition is passed** — this is not a
one-program mechanism. The actionable set, `SEPARABLE-UNRELATED`:

| program | count |
| --- | --- |
| life | 6 |
| sudoku5 | 5 |
| webserver | 4 |
| plcfrs | 4 |
| chull | 4 |
| sudoku4 | 2 |
| bh | 2 |

`sudoku5`, `plcfrs`, `sudoku4` and `chull` are four of the five flag-only
failures in [ifa/129](../ifa/issues/129-plan-demand-driven-creation-set-splitting.md);
`bh` and `life` are long-standing element-union cases. So the demand lands
on the right programs.

### What this changes downstream

**Steps 2-4 apply to SEPARABLE-UNRELATED only — 14 of 87.** The other two
populations need different answers and must not be fed to a splitter:

- **RELATED (11)** — hoist to the common ancestor, shedskin's model. A split
  here is wrong and `richards` is the proof.
- **FUSED (62)** — every writer already carries the union, so there is no
  key to split on. See below: these are mostly DOWNSTREAM of a confluence
  rather than 62 independent problems, so the count is not the size of the
  remaining work.

### What FUSED actually means — the union arrived pre-formed

Corrected 2026-09-14; the step-1 summary first treated FUSED as a separate
population needing its own answer. It mostly is not.

FUSED means **no two writers disagree cleanly**, so there is no key to
partition them by. A FUSED channel on `chull` (`cs=1876`):

```
ELEMWRITER es=312  __setitem__  Vertex#1191 Edge#1276 Edge#1896 Edge#1971 Edge#1972
ELEMWRITER es=313  __setitem__  Vertex#1191 Edge#1276 Edge#1896 Edge#1971 Edge#1972
ELEMWRITER es=576  __setitem__  Vertex#1191 Edge#1276 Edge#1896 Edge#1971 Edge#1972
ELEMWRITER es=671  __setitem__  Edge#1972
```

Most writers carry the whole union, and the one that does not (`es=671`,
pure Edge) OVERLAPS them rather than being disjoint, so no pair separates.
Contrast SEPARABLE `cs=1848`, where `es=680` writes only `Vertex` and
`es=497` only `Edge`.

**And those fused writers carry the identical five CreationSets, by id, as
`cs=1848`'s element.** They are not independently forming a union; they are
copying one that already exists. `cs=1876` is downstream of `cs=1848`.

So a FUSED channel is a VICTIM, not a source: its writers carry the union
because they read it from somewhere already contaminated. Fixing the sources
should collapse many of them, which makes the 62 an upper bound on the
remaining work rather than a count of it.

**But not all of them, and `linalg` is the case that shows it.** It has 30
FUSED and **0 SEPARABLE**, and all 30 are the same class set — `{int64, list}`
— one union shape propagated 30 times with no separable element confluence
to be its source. So the source is upstream in a form this census does not
look at: a function return, a field, or a formal. A fused channel with no
separable element source means *find the confluence one level up*, and
`{int64, list}` is a scalar/container mix, which is
[ifa/133](../ifa/issues/133-split-a-container-on-its-element-type.md)'s
residual family and issues/018's representation question rather than a
splittable class union.

## Step 2 — the blocker, found by walking the writers

Traced `chull`'s Vertex contamination back through `IFA_DBG_FUNES`. The
chain is three contours deep and says the same thing at every level:

```
__setitem__ es=497  recv=[list#1848 list#1887]  val=[Edge#1896]
__setitem__ es=680  recv=[list#1848 list#1887]  val=[Vertex#1191]
   <- append es=679 recv=[list#1848 list#1887]  val=[Vertex#1191]
      <- extend es=670 recv=[list#1848 list#1887] val=[Vertex#1191]
```

**The VALUE path is split at every level and the RECEIVER is shared at every
level.** `__setitem__` has one contour for the Edge write and another for the
Vertex write — the splitter did its job — but both carry the same receiver
`{list#1848, list#1887}`, so both writes land in both lists' element
channels.

And `list#1848` is in `Hull.edges` (`var=edges type= list#1848 list#1887`),
while the Vertex arrives through `extend`, which is `Edge.__init__`'s
`self.endpts.extend(endpts)`. **One list CreationSet is serving both
`Hull.edges` and `Edge.endpts`.** That is ifa/133's subject verbatim — a
merged container leaking elements between unrelated lists — reached from the
field-promotion side instead of the element side.

### Why ESBLOCK cannot be reused as-is

`PYC_ESBLOCK` finds the contour that blocks route 4 from partitioning a
CreationSet's `defs`. Its premise is that the defs exist but group into one.
Here `cs=1848` has **`defs=1`**: there is a single creation point, so there
is nothing for route 4 to partition and nothing for ESBLOCK's walk to
separate. The plan's step 2 assumed the walk would apply; it does not.

**The blocker is the CreationSet IDENTITY itself.** Two different fields are
served by one CS with one creation point, so no receiver-side split can
separate them — the receiver is literally the same object to the analysis.
Separating them needs a SECOND creation point, which needs the EntrySet that
creates it to split first. That is the dependency CLAUDE.md states, and it is
upstream of everything measured here.

*Stop condition reached, as written:* "if no candidate separates them, the
receiver is shared for a reason the walk cannot see, and that reason is the
next thing to find." The reason is `defs=1`, and the next thing to find is
why one creation point serves two fields — whether the two `[]` literals are
genuinely one site, or whether the def list is incomplete.

## Step 3's shape: push the demand back along the RECEIVER, not to a second creation point

Author: *"split the receiver by pushing the demand through or back analysis
from setters and splitting back to the creation point."* That is the right
direction and the target needs one correction, which the measurements pin
down.

**pyc already has the backward machinery and 133 already root-caused why it
stops.** The SETTER stage (`compute_setters` / `split_for_setters`) walks
backward over the CONTAINER graph, and the finding — kept from the compacted
history because it is the general rule — is:

> *types flow forward through the merge; setter attribution does not flow
> backward through it.*

`update_setter` walks `av->backward` from `x->container`, and a transfer
function that leaves no backward container edge stops it dead. `P_prim_merge`
was exactly that (every `__mul__` result def measured `back_closure=1`, an
empty backward set) and was fixed by teaching the walk to cross it, following
[146](../ifa/issues/146-remove-all-arbitrary-splitting.md) B's precedent of
teaching a backward walk to cross a folded global load. **So "push the demand
back through the setters" is an established, landed technique here — the
question is only where it has to reach.**

### shedskin does not split `Hull` either — it splits the LIST

Checked, because the premise matters: if shedskin needed two `Hull` contours
then pyc would too, and the target would be the ES above the creation point.
It does not. shedskin emits ONE `Hull` class and one instantiation
(`h = new Hull(sphere)`):

```cpp
class Hull : public pyobj {
    list<Vertex *> *vertices;
    list<Edge *> *edges;      // precisely typed -- no Vertex
    list<Face *> *faces;
};
```

The contour that differs in shedskin is the **list**, not the `Hull`: it
materializes `list<Vertex *>` and `list<Edge *>` as two distinct
parameterised types. pyc ALREADY HAS the corresponding two CreationSets —
`cs=1848` is `Hull.edges` with `defs=1`, which is correct — so the contour is
not missing. What is wrong is that a Vertex reaches it.

That is why the target is the receiver of the shared container methods and
not the ES above the creation point: there is no second `Hull` to make, and
no second `self.edges = []`.

### Where it has to reach is NOT a second creation point

`cs=1848` is created by `Hull.__init__` — verified, `es=82` has receiver
`Hull#1155` — and it has `defs=1`. That is CORRECT: there is one
`self.edges = []` in the program and one Hull. Splitting back "to the
creation point" cannot help, because the creation point is not ambiguous and
duplicating it would be inventing a second `Hull.edges` that the program does
not have.

**The ambiguity is in the RECEIVER FORMAL of the shared container methods.**
The chain measured in step 2:

```
extend es=670  recv=[list#1848 list#1887]  val=[Vertex]
  append es=679  recv=[list#1848 list#1887]  val=[Vertex]
    __setitem__ es=680  recv=[list#1848 list#1887]  val=[Vertex]
    __setitem__ es=497  recv=[list#1848 list#1887]  val=[Edge]
```

The VALUE split all the way down and the RECEIVER never did. `extend`'s
contour serves `Edge.__init__`'s `self.endpts.extend(endpts)` alongside
whatever reaches `Hull.edges`, and because its receiver formal unions them,
a Vertex written through it lands in the edges list's element channel.

**So the split to make is on the receiver formal of the shared container
method contour, propagated backward until each contour serves one container.**
That is "split the receiver" exactly as stated; the endpoint is a
single-container method contour, not a duplicated creation point.

### Why this is tractable where the earlier attempts were not

- It needs no new demand: the element confluence (step 1) already names it,
  and the classification says which ones are worth acting on
  (SEPARABLE-UNRELATED, 14 corpus sites).
- It needs no new backward walk: `update_setter`'s container-graph walk is
  the mechanism, and crossing an opaque transfer function is a technique this
  tree has already applied once and measured.
- The partition stays at 2: `{writers reaching Vertex}` vs
  `{writers reaching Edge}`, named by the demand.

### The receiver-cardinality measurement — defined, and taken

**What it is:** for every EntrySet, how many distinct CreationSets does its
RECEIVER formal hold? The receiver is `positional_arg_positions[1]` —
position 0 is the selector, as the `FUNES` dump shows
(`args= [__setitem__#44] [list#1848 list#1887] [int64#6] [Edge#1896]`).

**Why it gates the build:** the proposed split partitions a contour by its
receiver. If receivers routinely hold N containers, the split hands back N
groups and that is
[144](../ifa/issues/144-route-4-fans-per-creation-point-instead-of-partitioning.md)'s
fan — in the worst possible place, since `extend`/`append`/`__setitem__` are
the most-shared functions in any program. `IFA_DBG_RECVCARD`.

*(First cut reported `over1=0` on everything, because it picked the formal by
comparing MPosition POINTERS rather than using the ordered positional list.
Recorded because the wrong answer looked plausible — "every receiver is
already precise" — and would have made the whole risk vanish on paper.)*

**chull:**

```
total=556  receiver>1=92 (17%)  max=4 (__setitem__)
hist: 1=464  2=52  3=34  4=6
```

**Corpus, 76 programs:**

```
contours=29720  receiver>1=2756 (9.3%)  max=20 (enumerate)
hist: 1=26964  2=1236  3=428  4=192  5=135  6=289  7=95  8+=381
```

**The risk is real but narrow.** 90.7% of contours already have a
single-container receiver and need nothing. Of the 9.3% that do not, the
bulk are 2 or 3 (1664 of 2756), and the tail is genuinely long — 381
contours at 8 or more, with `enumerate` at 20.

So the rule that follows is not "split by receiver": it is **peel the
demanded group and leave the rest fused**, exactly as
[133](../ifa/issues/133-split-a-container-on-its-element-type.md)'s ESBLOCK
does with its "take exactly TWO groups" rule. On a receiver holding 20
containers the demand still names two — the one the Vertex reaches and
everything else — and a partition of 2 is what must be applied, never 20.
The tail is the reason that rule is mandatory rather than stylistic.

## Plan — find the confluence, create the demand, do the splits

Each step is measurable on its own, and each has a stop condition.

**1. Raise the demand at the ELEMENT CONFLUENCE, not at the field write.**
The MIXED field write (already recorded in `fieldsplit_demands`) is a
SYMPTOM three steps downstream; acting on it failed because its receiver is
a loop local with nothing to filter on. The demand belongs where the
confluence is: an element channel receiving two classes whose writers are
already separated. Record `(CreationSet, AType have, AType miss)` there.
*Stop condition:* if the corpus population of such channels is ~0 outside
`chull`, this is a one-program mechanism and should be scoped as such.

**2. Find the blocker by test, reusing `PYC_ESBLOCK`.** 133 already has it:
candidates are AVars on the backward walk that are FORMALS of a contour with
more than one in-edge; hold each terminal and recompute the per-writer
signatures; the one whose removal separates Vertex from Edge is the blocker.
Do not write a second copy — feed this demand into that walk.
*Stop condition:* if no candidate separates them, the receiver is shared for
a reason the walk cannot see, and that reason is the next thing to find.

**3. Split it into exactly TWO groups**, `{writers reaching Vertex}` vs
`{writers reaching Edge}`, by the same rule ESBLOCK uses — first signature
keeps the contour, everything else peels onto one product. The partition is
named by the demand and is 2 by construction, so it cannot become
[ifa/144](../ifa/issues/144-route-4-fans-per-creation-point-instead-of-partitioning.md)'s
fan. *Stop condition:* if the group count tracks the caller or member count,
stop — that is the fan, and it has been built and reverted twice already.

**4. Let route 4 finish the job.** After the ES split the site is reached in
two contours, `defs` becomes 2, and `CS_DEF_PARTITION` partitions the
element channels with machinery that already exists and is default-on. This
is the step that needs no new code, and it is the test of whether 1-3 did
their job.

*Verification for this step must include the FUSED COLLAPSE COUNT.* Re-run
`IFA_DBG_ELEMCONF` after the sources are separated and record how far the
62 fused channels fall. The prediction is that most go with their source,
since they carry the source's CreationSet ids verbatim. **If the count barely
moves, they are independent after all and need their own answer** — and
`linalg` is the standing counterexample to watch, with 30 fused, 0
separable, and every one the same `{int64, list}`.

**5. Then the promotion side becomes a cleanup, not a fix.** With the
element channel separated, the MIXED write stops arising, `chull`'s
cross-class promotion disappears, and the remaining work is the per-pass
`has` recompute so no residue survives from early imprecise passes — plus
the diagnostic shedskin has and pyc lacks, for unions that are genuinely
irreducible (`richards`).

### Verification

- `chull` compiles AND runs at the default and under the flip.
- `Hull.edges` element is `Edge` only; `IFA_DBG_LAYOUT` gives `Edge` and
  `Vertex` exactly their own five fields, matching shedskin's emitted C++.
- `richards` unchanged — its union is real and must survive.
- `./corpus_sweep.sh -m check` per program on both arms; `ess`/`css` not up.

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
