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

## The gap, stated honestly

Suppressing pass-0 union evidence was tried as a diagnostic
(`PYC_PROMOTELATE`, since removed). On the 14-line reproducer it **fixes it
completely** — `A` gets only `a`, `B` only `b`, output unchanged. On `chull`
it does **not**: the cross-class fields are still promoted. So a second path
reaches `unknown_vars` that `P_prim_setter` does not cover, and finding it is
the next step. Do not build the fix until it is found.

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

## Plan

1. **Find the second path into `unknown_vars`** — the gap above. Extend
   `IFA_DBG_PROMOTE` to every site that adds, not just `P_prim_setter`.
   Until this is done the rest is guesswork.
2. **Promote from the CONVERGED state, not from accumulated evidence.** The
   principled rule: a field is promoted onto a class because a write to it
   was observed *at the fixed point*, not because a transient union at pass
   0 briefly made it look possible. `unknown_vars` is already cleared per
   pass; what is missing is that `reanalyze` consumes whatever survived the
   last pass rather than re-validating it.
3. **Then decide the layout question separately.** Even with (2), a
   legitimate union of unrelated classes read through one receiver has no
   sound blind cast. The options are shedskin's — hoist to a shared base
   (a REPRESENTATION property, legitimate ground) — or refuse. Do not
   reach for a better sort order: that is making a shared layout by
   coincidence, and it cannot work when the base offsets differ.

## What this unblocks

`chull` under the start-merged flip ([ifa/129](../ifa/issues/129-plan-demand-driven-creation-set-splitting.md)
step 2), and `chull`'s pre-existing runtime segfault on main, which is the
same defect going undiagnosed.
