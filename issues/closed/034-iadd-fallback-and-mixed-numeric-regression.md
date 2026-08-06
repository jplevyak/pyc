# 034 — Augmented assignment has no `__i<op>__` → `__<op>__` fallback; fixing it surfaced a regression in issue 077's own recent fix

**Status: FIXED 2026-08-06.** Found investigating
`shedskin_examples/yopyra/yopyra.py`, the second of two independent
core issues in that file (the first,
[033](033-comprehension-filter-and-or-boolean-context-gap.md), was a
separate fatal compile crash).
**Affects:** `python_ifa_build_syms.cc`'s `gen_class_pyda` (new
`synthesize_default_iop` helper) and, for the regression found while
verifying, `ifa/codegen/cg.cc`'s `emit_send_default_prim`.
**Related:** [ifa/issues/closed/077](../../ifa/issues/closed/077-primitive-equality-codegen-missing-salvage-guard.md)
— the type-mismatch guard this issue's verification pass found a real
false-positive in was added by 077, in a prior session; that guard's
own regression test suite never happened to exercise a case that
tripped it (see "The regression" below).

## Part 1: `__iadd__`/`__isub__`/etc. have no fallback to `__add__`/`__sub__`/etc.

### Symptom

`yopyra.py`'s `color` and `punto3d` classes define `__add__` (etc.)
but not `__iadd__` — it's literally commented out in the source.
`c += luz.color` fails:

```
warning: unresolved call '__iadd__'
warning: illegal call argument type 'c' illegal: color
...
warning: expression has no type
```

which degrades to a runtime `assert(!"runtime error: matching
function not found")` — compiles, but is silently wrong at runtime
for every `+=` (and `-=`, `*=`, ...) against such an object.

### Root cause

CPython's data model falls back from `__i<op>__` to `__<op>__` (then
reassigns) whenever a class defines the non-in-place operator alone —
`c += x` is exactly `c = c.__add__(x)` when `__iadd__` doesn't exist.
This is *not* opt-in in real Python; it's the universal default for
every class, and most classes never bother writing a separate
in-place method solely to support `+=`. `python_ifa_build_if1.cc`'s
`PY_augassign` case always sends `map_pyop_to_ioperator(op)` (e.g.
`__iadd__`) directly, with no such fallback — a general gap, not
`yopyra.py`-specific: *any* class defining only `__add__` breaks on
`+=` against it.

### Fix

`gen_class_pyda` already auto-synthesizes methods a class doesn't
define its own version of — `__deepcopy__` (issue 029) and, opt-in,
the comparison family (issue 068's `synthesize_derived_compare`, only
under `@pyc_compare` since Python's *real* default for `__eq__`/
ordering is identity/unimplemented, unlike `+=`'s fallback). Added
`synthesize_default_iop`, the same shape as `synthesize_derived
_compare` but simpler: when a class defines `__<op>__` but not its
own `__i<op>__`, synthesize `def __i<op>__(self, other): return
self.__<op>__(other)`. Unlike the comparison family, this runs
**unconditionally** (no flag) — it reproduces CPython's own
unconditional default, not a new opt-in behavior. Covers all 12
operator pairs (`__add__`/`__sub__`/`__mul__`/`__truediv__`/
`__mod__`/`__pow__`/`__lshift__`/`__rshift__`/`__or__`/`__xor__`/
`__and__`/`__floordiv__` and their `__i...__` counterparts). Scoped to
`is_record` classes (matching `__deepcopy__`'s and the compare
family's own scoping) and to a class's *own*, directly-defined
`__<op>__` (mirrors `synthesize_derived_compare`'s existing "skipped
if the class defines its own" check, same own-scope-only precedent —
inherited-only `__<op>__` with no own `__iadd__` is left unaddressed,
same as before, not a regression).

## Part 2: verifying the fix surfaced a real bug in issue 077's own recent guard

With `__iadd__` synthesis landed, `yopyra.py` compiled with **zero**
warnings (was ~84) — but the compiled binary aborted at runtime:

```
Assertion `!"runtime error: primitive operand type mismatch"' failed.
```

Traced to `int.__mul__`'s `__pyc_operator__` fallback (`__pyc__/
02_numeric.py`) hitting a genuine `2 * <float>` — ordinary, correct
mixed-numeric multiplication. `cg.cc`'s `emit_send_default_prim`
(added by issue 077, prior session) compares operand C types via
`strcmp` on their raw `c_type()` *strings* — `"_CG_int64" !=
"_CG_float64"`, so this legitimate operation was flagged as a
mismatch, exactly the false-positive class 077's `c_call_codegen` fix
had already needed a `num_kind`-based tolerance to avoid, but that
tolerance was never carried over to this sibling call site. Neither
`test_pyc.py` nor the corpus sweep happened to exercise a case that
reached this exact fallback with genuinely different-but-compatible
numeric types before `yopyra.py` (only reachable *after* Part 1's fix
unblocked further compilation).

**Fix:** rewrote the check to tolerate any two scalar operands
(`num_kind` truthy on both, any kind/width) as compatible — mirrors
`cg_emit_llvm.cc`'s `emit_send_binop`, which already actively coerces
int↔float/int-width pairs rather than erroring, and matches
`c_call_codegen`'s own tolerance from the same original issue. Only a
scalar paired with a non-numeric (pointer-like) operand, or two
non-numeric operands with genuinely different C representations, is
still flagged.

## Verification

- `tests/augassign_fallback_no_iadd.py` (new): a class with only
  `__add__`/`__sub__`/`__mul__` exercises `+=`/`-=`/`*=`; a second
  class that DOES define its own `__iadd__` confirms the real
  override still wins over the synthesized fallback (not silently
  replaced). Output matches `python3` exactly.
- `tests/mixed_numeric_binop.py` (new): `int`/`float` arithmetic and
  comparison across `+`, `-`, `*`, `==`, `<`. Compiles with zero
  warnings, output matches `python3` exactly.
- `yopyra.py`: compiles with **zero** warnings on both backends (was
  ~84); the compiled binary runs (a real raytrace against `scene.txt`,
  no crash) instead of aborting at the first `int.__mul__` call.
- `ifa --test`: 58/58.
- `test_pyc.py`, C and LLVM backends, `PYC_CSM` unset: 240/11/0/4 both
  (238 baseline + 2 new tests, 0 regressions).
- `test_pyc.py`, C and LLVM backends, `PYC_CSM=2`: 236/11/4/4 both,
  same 4 pre-existing failures.
- `shedskin_sweep.sh`, both `PYC_CSM` settings: one clean gain each
  (`yopyra`: `COMPILED_C_WARN` → `COMPILED_C`), zero regressions,
  diffed directly against saved pre-fix `results.tsv` for both the
  `__iadd__` change and the `emit_send_default_prim` correction
  separately.

## What this unblocks

`+=`/`-=`/`*=`/etc. against any class that defines only the
non-in-place operator — an ordinary, common pattern, not an edge
case — now behaves correctly instead of aborting at runtime. Not
`yopyra.py`-specific. The `emit_send_default_prim` correction fixes
ordinary mixed-numeric arithmetic (`int * float` and friends) reaching
that fallback anywhere in the corpus, not just in the class newly
unblocked by Part 1.
