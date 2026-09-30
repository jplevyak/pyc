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
2. **Promoted fields are derived state.** Mark them (`Sym` has bitfield
   room for `is_promoted_field`), reset them wherever `unknown_vars` is
   reset, and re-derive them each pass. A transient pass-0 union then
   leaves nothing behind. Check first: does anything cache a `has` index
   mid-analysis? And how does `clone.cc`'s post-convergence `has` rebuild
   (858/908/1453) interact?
3. **The three-way rule for a union receiver** (measured with
   `IFA_DBG_FIELDSPLIT`, `PYC_FIELDSPLIT` scaffolding in the tree):
   - ALL-HAVE: flow normally.
   - ALL-MISS: promote, as derived state (step 2). `richards` needs
     this; its 24 are real.
   - MIXED (one class has the field, others do not): this is a DEMAND to
     separate the union, {have} vs {miss}, exactly two groups. Acting at
     the receiver was measured useless (162 demands, 0 splits: the
     receivers are loop locals). It must feed the ESBLOCK blocker walk
     (ifa/129), which reaches the shared writer contour.
4. **A genuine union of unrelated classes** read through one receiver
   needs a diagnostic naming it (shedskin warns `dynamic (sub)type`), or
   hoisting to a real common base. Never a coincidental shared layout.

## Verification

- The 14-line repro: `A` keeps only `a` (`IFA_DBG_LAYOUT`).
- `chull` and `richards` unchanged (both run). `richards`' union is real.
- With `discovers_fields_by_write()` false, the ifa V-language tests pass
  unchanged (V declares its fields).
- Same-binary corpus `check`: no verdict change; `css`/`ess` not up.
