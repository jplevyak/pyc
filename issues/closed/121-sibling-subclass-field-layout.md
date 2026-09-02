# 121 — sibling subclasses of one base get inherited fields at different struct slots

**Status: CLOSED 2026-09-02** — fixed 2026-08-30, archived after
re-verifying: `tests/sibling_subclass_field_layout.py` is in the suite
with no `.known_issue` sidecar and passes (suite green, 308/17/0).

**FIXED 2026-08-30**, filed the same day while root-causing
[120](../120-richards-silent-wrong-answer.md). `promote_field` now promotes
a CreationSet's pending fields in name-sorted order, so sibling classes
agree on slot assignment. It briefly cost a `.known_issue` on
`tests/deepcopy_objects.py` — once sibling classes stop getting
different field orders they become genuinely equivalent, and
[ifa/112](../../ifa/issues/closed/112-CGEN-nondeterministic-emitted-c.md)'s
pointer-hashed tiebreaks started deciding between them. **That cost is
gone**: the same session fixed the FA-side source (violation iteration
order), and `deepcopy_objects` is deterministic across 8 compiles with
no sidecar.
**Affects:** `python_ifa_sym.cc` (`promote_field`),
`ifa/analysis/clone.cc` (`determine_layouts`, `compute_member_types`).
**Severity:** silent — zero warnings, exit 0, wrong values.
**Reproducer:** `tests/sibling_subclass_field_layout.py`, 22 lines
(passing since the fix; no sidecar).

## Symptom

```
cpython: 11 11 12 13 1 | 21 21 22 23 2
pyc    : 11 11 12 13 1 | 23 23 22 21 2
```

`S1` and `S2` both extend `Base` and declare no fields of their own;
`a`, `b`, `c` are assigned in `Base.__init__`. S2's come back REVERSED.

## Root cause

A field assigned only through a base's `__init__` is not in the class's
AST `has` list at all. It is added during flow analysis by
`promote_field`, which **appends** to `cs->sym->has`:

```
PROMOTE S1.a -> index 15      PROMOTE S2.c -> index 15
PROMOTE S1.b -> index 16      PROMOTE S2.b -> index 16
PROMOTE S1.c -> index 17      PROMOTE S2.a -> index 17
```

The append order comes from iterating `cs->unknown_vars`, a hash set.
The `has` index **is** the emitted struct's `eN` suffix (`cg.cc`), so
the two classes get the same names at different slots:

```c
struct _CG_s11999 { ... _CG_int64 e15; /* a */ e16; /* b */ e17; /* c */ };
struct _CG_s12000 { ... _CG_int64 e15; /* c */ e16; /* b */ e17; /* a */ };
```

and every access casts the union receiver to ONE of them
(`((_CG_ps11999)t)->e15`).

## Why nothing catches it

`determine_layouts` walks fields **name-sorted** to compute the byte
offsets that `prim_period_offset` validates ("mismatched offsets"). The
emitter uses `cs->vars` / `has` order instead. **The check passes on an
ordering the emitter does not use**, so the two never disagree where it
would be noticed. Related to
[ifa/118](../../ifa/issues/118-union-field-representation-and-polymorphic-field-offset.md)'s
`go` half ("one field name, two classes, two offsets"), which is the
same disagreement caught at compile time instead of miscompiled.

## The fix, and what it costs

Sorting `cs->unknown_vars` by name before promoting makes both classes
promote `a, b, c` in order. Measured:

- `tests/sibling_subclass_field_layout.py` matches CPython.
- richards (issues/120) goes from a garbage scheduler trace
  (`ident` reading 3000, then 0, then `-44889163002019841`) to one
  **identical to CPython's for every traced iteration**.
- Whole suite otherwise unchanged, both backends.

**It initially destabilised `tests/deepcopy_objects.py`** — the NONDET check
(same source, two different `.c` files) starts failing about half the
time, where the baseline is stable 5/5. The reason: promotion order
varies across reanalyze **ROUNDS**, not only within one, so sorting
within a round does not make the layout order-independent; it only
changes which arbitrary order you get, and once two sibling CSs become
genuinely equivalent, `ifa/112`'s pointer-hashed representative
tiebreak starts choosing between them. Making the eqcss representative
canonical by `id` (tried: `canonical_in_set`, both in
`compute_member_types` and `sets_by_f_transitive`) removes one such
tiebreak but not the round-to-round variation upstream.

## What a COMPLETE fix looks like

The landed fix makes the order canonical *within a reanalyze round*,
which is enough for sibling classes that acquire the same field set.
Order-independence needs the canonicalization the field order **once, after FA settles and before
`clone`**, rather than at promotion time — then it cannot depend on
round structure. That means permuting `cs->vars` and the class's `has`
together, since `compute_member_types` aligns them positionally
(`sym->has.fill(cs->vars.n)`, `has[i]` naming `vars[i]`).

The complication, and why this is not a small edit: CSs of the SAME
class may legitimately hold different field SETS (`promote_field`'s own
comment says so, and `determine_basic_clones` relies on it to give them
distinct struct types). A single permutation of the shared `has` cannot
serve all of them, so the canonical order has to be computed per
equivalence class, not per class Sym.

Name-sorting alone is also not sufficient in general: a base with
`{a, c}` and a subclass with `{a, b, c}` still disagree on `c`'s index.
The full invariant is the one `ast.cc`'s `collect_include_vars` already
states for AST-declared members — **a subclass's layout must be a
prefix-compatible extension of its base's** — extended to cover
promoted fields.

## Verification

- `tests/sibling_subclass_field_layout.py` matches CPython on both
  backends (landed, no `.known_issue`).
- richards' scheduler trace now matches CPython for every traced
  iteration — [120](../120-richards-silent-wrong-answer.md) needs more
  than this and stays open.
- All five gates green.

- `tests/deepcopy_objects.py` deterministic across 8 compiles, with no
  `.known_issue` — the FA-side ordering fix that made this possible is
  recorded in ifa/112.
