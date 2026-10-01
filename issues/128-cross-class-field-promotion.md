# 128 — "a write discovers a field" is a Python-ism in generic ifa, and its result is never re-derived

**Status:** open. Rewritten 2026-09-28. The union that made it reachable
on `chull` is fixed by closed
[ifa/152](../ifa/issues/closed/152-FA-backtrack-the-demand-to-the-merged-creation-set.md),
and chull compiles and runs. The promotion mechanism this file is about is
unchanged. The full record, including the four failed attempts (AGENTS.md
cites them as its worked example of acting anywhere but the confluence),
is in git: `git show e3b44e2c:issues/128-cross-class-field-promotion.md`.

## The mechanism

`P_prim_setter` (generic `ifa/analysis/fa.cc`) handles `obj.f = v` by
visiting every CreationSet in the receiver's type:

```c
AVar *iv = cs->var_map.get(symbol);
if (iv) flow_vars(tval, iv);
else    cs->unknown_vars.add(symbol);   // a write to a missing field DISCOVERS it
```

pyc's `promote_field` (`python_ifa_sym.cc`) then appends to
`cs->sym->has`, and that index IS the emitted struct's `eN`. Two defects:

1. ~~**It is Python semantics hard-coded in ifa.**~~ Now behind
   `IFACallbacks::discovers_fields_by_write()` (step 1 below).
2. **The conclusion outlives its evidence.** `unknown_vars` is cleared
   every pass (`clear_cs`), but `sym->has` is never cleared. On `chull`,
   all 777 promoting writes happen at pass 0, through a union that is
   gone by pass 1, yet the fields persist. Two unrelated classes then
   hold each other's fields at colliding offsets (`Edge.onhull` 22 vs
   `Vertex.newface` 22). The 14-line repro (`xs = [A(), A()]`,
   `ys = [B(), B()]`) gives `A` and `B` both fields.

## Fix, in order

1. ~~**`IFACallbacks::discovers_fields_by_write()`**~~ **DONE
   2026-09-29** (`ifa/ifa.h`; `PycCallbacks` returns `true`). With it
   `false` (ifa's default), a write to a missing field is a MEMBER
   violation instead of a promotion (`fa_prims.cc`, `P_prim_setter`). No
   behaviour change for pyc. The V-language tests (`ifa --test`, all 16
   `test-ir` phases) pass unchanged with the default, as predicted: V
   declares its fields. This was
   [171](closed/171-permissive-accommodations-must-be-flagged-and-non-strict.md)
   #8.
2. ~~**Promoted fields are derived state.**~~ **DONE 2026-09-30.**
   `IFACallbacks::retract_derived_state()` (`ifa/ifa.h`) is called at a
   fixed point, where the driver already runs its narrowing
   (`cselem_resplit_diverged`): only when no other stage asked for a pass.
   pyc's implementation (`python_ifa_sym.cc`) withdraws a promoted field
   from its class, and from every CreationSet of the class, when **no**
   CreationSet of the class received a value for it in the converged pass.
   - **Per class, not per CreationSet.** The first version retracted per
     CreationSet and broke `kanoodle`: two clones of `Column` disagreed on a
     slot ("'Column' is blind-cast to 'Column' ... member width differs").
     A field is part of the class's layout, so all clones must agree.
   - **Termination.** A field withdrawn and then promoted again is pinned,
     because its evidence reappeared once it was gone. So a field is
     withdrawn at most once.
   - **The pre-checks.** Nothing caches a record's `has` index during
     analysis; the `has.n`/`has[i]` uses in `fa.cc` are pattern and tuple
     arities. `clone.cc`'s `has` rebuild runs after convergence, on the
     retracted set.
   - **Measured.** The 14-line repro: `A {a}`, `B {b}` in the emitted
     structs (`tests/promoted_field_retracted.py` pins the two retractions
     via `IFA_DBG_RETRACT`; it fails with `PYC_NORETRACT=1`). `bh` drops
     `Cell.acc` and `Cell.vel` (the residual closed/039 traced here).
     `chull` drops 18 cross-class fields, and `chull` and `bh` match CPython.
     `richards` prints byte-identical output to the previous build. Corpus
     `check`: every verdict list identical to the baseline, with CreationSets
     and EntrySets unchanged (sweep `check__default__11a77416+*`).
     `PYC_NORETRACT=1` turns it off.
3. ~~**The three-way rule for a union receiver**~~ **BUILT 2026-09-30.**
   A MIXED write is now a demand, asked in `split_css_by_defs` on the
   CONVERGED types from that pass's writes (`record_field_write` in
   `P_prim_setter`). A receiver's record classes split into those with
   their OWN evidence for the field (a write through a receiver of that
   single class) and those without. Its value flow is walked backward, and
   every CreationSet on it is named and treated as demanded (like
   `viol_named`), so the ES-block, defs==1 and route-4 rungs partition the
   CreationSet that merged, or decline. `IFA_DBG_FIELDMIXED` prints each
   demand once.
   - **Why "own evidence" and not the probe's have/miss.** At convergence
     the promotion itself gives every member the field, so every MIXED write
     looks ALL-HAVE. That is why step 2's measurement found none.
   - **Two constraints, both measured.** The first version named EVERY
     CreationSet on the backward walk, on every pass. On `plcfrs` a single
     pass-0 `'count'` write spans seven classes. It named 184 CreationSets
     and the cascade added 306 (+66 EntrySets) with no verdict change: the
     fan. So the demand (a) names only the CONFLUENCE, a CreationSet whose
     content still carries a class with its own writer AND one without;
     that alone still named 93, because everything mixes on pass 0. And (b)
     it is asked only when no higher stage acted this pass (`quiescent`),
     since a union a higher stage is still separating is not yet the
     program's. With both, the corpus `check` (`check__default__888c8502+ee4a19cc`)
     shows ZERO programs changing contour counts against step 2 and no
     verdict change.
   - **What it acts on today: nothing.** Every converged MIXED write I could
     construct is a GENUINE union. A module global holds both classes over
     time and names no CreationSet. An attribute reassigned over time names
     one CreationSet, with one creation point. A heterogeneous list is one
     literal. The rungs decline, correctly, and the output matches CPython.
     Every shared-contour shape tried was separated before convergence by
     existing machinery (setter splitting, route 4). That includes two lists
     from one helper, two literals filled through one helper, one `Bag`
     class per list, and one `Node` wrapper per object; none of 9 splitter
     flags set to 0 leaves a MIXED write. The corpus has no converged MIXED
     write either (step 2's measurement, over 76 programs).
   - **A narrowing gap, found on the way:** an `isinstance` guard does not
     narrow a MODULE-LEVEL variable, because each read is a fresh load of the
     cell. So `if isinstance(x, A): x.f = 4` at module level is MIXED where
     the same code in a function is not. Filed as
     [ifa/177](../ifa/issues/177-FA-narrowing-does-not-reach-a-module-global.md);
     it is a narrowing fix, not a split.
   - **Replaced:** the `PYC_FIELDSPLIT` scaffolding. It split the receiver's
     own contour by type, which was measured useless, and it read a demand
     list that the split stage cleared before using it.
   - **Test:** `tests/field_mixed_write_demand.py` pins, through its `.env`
     and `.check`, which writes raise the demand: the temporal attribute and
     the module global do, the narrowed local does not. It pins that all
     three print what CPython prints.
4. **A genuine union of unrelated classes** read through one receiver
   needs a diagnostic naming it (shedskin warns `dynamic (sub)type`), or
   hoisting to a real common base. Never a coincidental shared layout.

## Verification

- The 14-line repro: `A` keeps only `a` (`IFA_DBG_LAYOUT`).
- `chull` and `richards` unchanged (both run). `richards`' union is real.
- With `discovers_fields_by_write()` false, the ifa V-language tests pass
  unchanged (V declares its fields).
- Same-binary corpus `check`: no verdict change; `css`/`ess` not up.
