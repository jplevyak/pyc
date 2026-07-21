# Issue 026: Polymorphic method dispatch crashes (C) / silently drops calls (LLVM) when not every union member overrides the method

**Status:** closed 2026-07-13, fixed both backends.
**Affects:** method-call dispatch codegen — `ifa/codegen/cg.cc`
(`emit_send_call`'s classtag-branch construction, C backend),
`ifa/codegen/cg_emit_llvm.cc` (same shape, LLVM backend), and
`ifa/codegen/codegen_common.cc` (`cg_build_new_to_val_map`, the
shared method-pointer-slot registry both backends' `P_prim_clone`
slot-store emission reads from).
**Related:** [ifa/issues/030-polymorphic-dispatch-fat-pointers.md](../../ifa/issues/030-polymorphic-dispatch-fat-pointers.md)
(the classtag dispatch mechanism this bug lived in); found while
stress-testing
[closed/003-subclass-struct-layout-mismatch.md](003-subclass-struct-layout-mismatch.md)
(unrelated to that issue's struct-layout root cause — this
reproduces with zero data fields involved, pure method dispatch).
**Not related to
[023-structural-pattern-matching.md](../023-structural-pattern-matching.md)'s
`case None:` limitation**, despite the identical assertion text —
see "Ruled out: NOT the same bug as issue 023's `case None:`
limitation" below. That was the leading hypothesis when this issue
was first filed; investigating THIS issue's actual root cause (a
receiver formal typed as a union of INHERITING classes) made it
possible to test that hypothesis directly, and it doesn't hold.

## Symptom

```python
class Shape:
    def describe(self):
        return "shape"
class Circle(Shape):
    def describe(self):
        return "circle"
class Square(Shape):
    pass          # inherits Shape.describe unchanged -- no override

items = [Circle(), Square(), Shape()]
for it in items:
    print(it.describe())
```

CPython prints `circle`, `shape`, `shape`. Before this fix, on pyc:

- **C backend**: compiled, then aborted at runtime:
  ```
  methodonly.py.c:379: _CG_nil_type _CG_f_107_18(): Assertion
    `!"runtime error: matching function not found"' failed.
  ```
- **v2 LLVM backend**: compiled AND ran to completion with exit
  code 0, but silently printed **nothing** — the more dangerous
  variant: no crash, no diagnostic, just missing output.

Minimal repro needs only ONE class with no override at all in the
union (`Square`, above, or the base `Shape` instantiated directly).

## Root cause (confirmed, not just hypothesis)

The ORIGINAL filed hypothesis ("a class that doesn't override the
method presumably doesn't get its own slot-holding candidate branch
constructed") pointed at the right neighborhood but not the actual
mechanism. Confirmed via `PYC_DBG_DISPATCH=1` plus ad hoc tracing
(temporarily printing each rt-search candidate's `type_kind`):

FA gives a method Fun's `self` formal a **`Type_SUM` (union) Sym**
when the SAME Fun is reached by multiple concrete classes through
inheritance — here, `Shape.describe`'s self formal is typed
`Square | Shape` (Square has no override of its own, so calling
`square_instance.describe()` and `shape_instance.describe()` both
resolve to the literal same Fun). `Circle.describe`, having its own
distinct implementation, keeps a plain `Circle` self type.

The classtag-branch-construction code in BOTH backends' `emit_send_call`
(and the shared slot-registration code in `codegen_common.cc`'s
`cg_build_new_to_val_map`) searched a receiver formal's type for a
live field named like the method — e.g. `csym->has[k]->name ==
method_name`. This works for a `Type_RECORD` (`csym->has` is that
class's own fields/methods) but **silently fails for a `Type_SUM`**:
a union Sym's `.has` holds its MEMBER TYPES (`Square`, `Shape`
themselves as Sym pointers), not fields — `csym->has[k]->name` is
`"Square"` or `"Shape"`, never `"describe"`, so the search always
came up empty for a method reached only through inheritance. With
`rt` never found for `Shape.describe`, that candidate fell out of
classtag routing entirely, `Square`'s (and, depending on candidate
order, `Shape`'s own) instances got no dispatch branch at all, and
calls to them hit the "no branch matched" fallback — the C
backend's `assert`, the LLVM emitter's silent no-op.

## The fix

Three call sites needed the identical shape of fix — recognize when
a receiver formal's type is `Type_SUM` and recurse into its member
Syms (searching EACH member's own `.has` for the method, since each
member's own dead-field-elision layout can put the same method at a
different index — issue 026's own earlier text already flagged this
possibility: "leaf structs carried val at e1, Inner at e2"):

1. **`cg.cc`'s `emit_send_call`** (C backend): the rt-search now
   collects potentially SEVERAL concrete receiver classes per
   candidate Fun (`Vec<Sym*> rts` instead of a single `Sym *rt`),
   emitting a classtag branch for each.
2. **`cg_emit_llvm.cc`'s `emit_send_call`** (LLVM backend): the
   identical change, mirroring the C backend's shape exactly.
3. **`codegen_common.cc`'s `cg_build_new_to_val_map`**: the
   `Fun* -> Vec<PolymorphicSlot>` registry both backends'
   `P_prim_clone` slot-store emission reads to populate each
   instance's stored method pointer at construction time had the
   IDENTICAL bug in its own self-arg search — meaning even after
   fixing (1) and (2) to emit a dispatch branch for `Square`, that
   branch read an uninitialized/garbage slot (the C backend happened
   to still work here because instance method-pointer values are
   ALSO populated via a separate, class-prototype-level mechanism
   this registry doesn't gate — but the LLVM backend's
   `P_prim_clone` emission relies on this registry directly, so the
   dispatch branch existing wasn't enough there: it read garbage and
   **segfaulted** on the CORRECT branch now being taken).

**A second, independent bug found while fixing (3) and landing
regression tests**: `poly_dispatch_low.py` / `poly_dispatch_high.py`
(pre-existing tests, both concrete classes in their unions DO
override the dispatched method — no inheritance-sharing at all)
started segfaulting once the slot lookup in `cg_build_new_to_val_map`
was changed to re-resolve the slot per concrete class unconditionally.
Root cause: for a NON-union self arg, re-resolving the slot from
`cs->sym` (a CreationSet's own concrete class, which can differ from
the self arg's own static type since FA's AType can be broader than
the formal's declared type) picked a DIFFERENT, wrong slot than the
one the rest of the pipeline expected, corrupting dispatch for
classes that already had their own, correct, non-union candidate.
Fixed by keeping the ORIGINAL single-slot behavior exactly when the
self arg is NOT a union, and only doing the per-class (per `cs->sym`)
re-resolution when it genuinely is one.

**A third, related correctness gap found alongside (2)**: even with
the `Type_SUM` unpacking working, a DIFFERENT bug surfaced —
`poly_dispatch_low.py` first regressed at the DISPATCH-CONSTRUCTION
level too (not just the slot registry): a class that already has its
OWN override among the call's candidates could get silently
overridden by a DIFFERENT candidate's looser (union-typed) match,
misdispatching it to the wrong implementation. Fixed with a pre-pass
in both `cg.cc` and `cg_emit_llvm.cc`: compute which classes are
DIRECTLY, singularly owned by some candidate (a non-union self type)
before unpacking any union, and never let a union-unpacked match
steal a class that's already directly owned by another candidate.

## Verification

- `tests/poly_dispatch_partial_override.py` (this issue's own repro,
  Shape/Circle/Square) — new test, `.exec.check` = `circle\nshape\nshape\n`,
  verified byte-identical to `python3` on BOTH backends.
- `tests/poly_dispatch_low.py`, `tests/poly_dispatch_high.py`,
  `tests/poly_dispatch_swapped.py` (pre-existing issue 030 tests) —
  all still pass strictly on both backends (these were the tests
  that caught the two regressions described above during development
  — the fix wasn't considered done until they passed again).
- Full suite: 188/0 on both the C and LLVM backends (`test_pyc.py`
  and `PYC_FLAGS=--llvm test_pyc.py`), `make test_dparse` all pass,
  `make -C ifa test_llvm` passes, `ifa/ifa-test` 17/17.

## Ruled out: NOT the same bug as issue 023's `case None:` limitation

Issue 023 documents a separate, still-open limitation: `case None:`
combined with almost any other `match`/`case` pattern in the same
statement crashes with the IDENTICAL assertion text
(`Assertion '!"runtime error: matching function not found"'`).
Before this issue was root-caused, that looked like a plausible
second manifestation of the same bug. With this fix landed, it's
possible to test that directly: temporarily bypassing issue 023's
compile-time refusal and compiling `case None: / case 5: / case n:`
still crashes, with `PYC_DBG_DISPATCH=1` showing the failure is in
`__str__` dispatch (from `print()` formatting the capture-bound
`n`), not a class-method-inheritance classtag gap at all — `n`'s
type spans `None | int | float`, none of which carry a classtag
(`cg_has_classtag` requires `Type_RECORD`; `int`/`float`/`None` are
primitive/system types), so this is dispatch over a UNION OF
PRIMITIVE/BOXED TYPES, a mechanism this issue's fix doesn't touch at
all. Genuinely a different, still-unfixed gap — issue 023's own doc
has been updated to drop the "likely same root cause" framing.
