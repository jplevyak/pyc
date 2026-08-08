# 035 — `P_prim_set_index_object` casts the assigned value with no compatibility check; a genuine `set`-element type union remains open

**Status:** partially fixed 2026-08-06. The fatal compile-error part
(both branches of `P_prim_set_index_object`) is fixed and verified.
`shedskin_examples/tictactoe/tictactoe.py` now compiles clean with
zero warnings on both backends — it does **not** yet run to
completion; see "What's still open" below. A second, independent
fix (`__pyc_clone_constants__` on `__dict_iter__`/
`__dict_items_iter__`/`__set_iter__`'s constructors) closes the
`webserver.py` regression a first attempt at the `set`-element-union
gap caused — see "The `webserver.py` regression: root-caused and
fixed" below.
**Affects:** `ifa/codegen/cg.cc`'s `P_prim_set_index_object` (both
the general list branch and the fixed-size tuple-list constant-index
branch).
**Related:** [056](../ifa/issues/closed/056-CGEN-degraded-index-type-raw-c-compile-error.md) —
the established precedent and convention this issue extends to a new
call site (the *value* being stored, not the index argument, which
056 already covers at the same two call sites);
[077](../ifa/issues/closed/077-primitive-equality-codegen-missing-salvage-guard.md)/[034](closed/034-iadd-fallback-and-mixed-numeric-regression.md)
— the same "num_kind-based scalar/pointer tolerance" pattern, applied
here for the third time at a fourth call site.

## Symptom

`tictactoe.py` failed to compile:

```
tictactoe.py.c:3972:88: error: cannot cast from type '_CG_float64' (aka 'double') to pointer type '_CG_void' (aka 'void *')
 3972 |   ((_CG_void*)(_CG_list_ptr(t155)))[_CG_norm_idx(t92,(int32)_CG_prim_len(0,t155))-0] = (_CG_void)t156;
```

Ten occurrences, all the same shape: a list whose element type
resolved to `_CG_void` (this list's storage layout, e.g. a
salvage-degraded or genuinely heterogeneous union collapsed to a
generic/boxed representation) receiving a value that resolved to a
concrete scalar (`_CG_float64`). Casting a `double` directly to a
pointer type is invalid C (unlike two pointer types, or two scalars
of different width/kind, which are always castable) — a genuine
`pyc`-produced compile error, the same bug *class* 056 already named
and fixed for the *index* argument at these same two call sites, now
found at the *value* argument instead.

## Root cause

`P_prim_set_index_object` (`cg.cc`) emits `((ety*)(...))[idx] =
(ety)value;` — casting the assigned value directly to the list's
element C type — with no check that the value's actual resolved type
is compatible with that cast. Reachable via two branches: the general
(dynamic list/negative-index-normalized) branch, and a separate
fixed-size tuple-list constant-index branch (`((T)list)->e<N> =
value;`) for lists whose literal size makes them internally
struct-like — both had the identical unguarded-cast shape.

## Fix

Both branches now check, before emitting the cast: is exactly one of
{element/field type, value's resolved type} a scalar (`num_kind`
truthy) and the other not? If so — a genuine pointer/scalar
mismatch — degrade to `assert(!"runtime error: list element type
mismatch")` (or `fail(...)` when `fruntime_errors` is off) instead of
emitting the cast. Two scalars of any kind/width, or two
pointer-representable types, are always compatible (mirrors
077/034's identical tolerance at sibling call sites) — only the
scalar-vs-pointer crossing is flagged.

## What's still open

With the compile fix alone, `tictactoe.py` compiles clean but the
binary aborts at runtime on this same new guard.

### Root-caused, 2026-08-06: not a `set` bug at all — a general heterogeneous-list gap, `set` is an innocent bystander

The `set`-element-union framing above was wrong about *where* the
float comes from (it does not come from `union()`/`intersection()`/
`__pyc_set_from_iterable__`'s method-table-installation-time
analysis — that theory was never confirmed and turned out to be a
red herring). Root-caused by direct reduction of `tictactoe.py`
itself (progressively stripped in a scratch copy, not the committed
example) rather than by further hand-decoding the generated C:

**The minimal repro has nothing to do with `set`, classes, or
tictactoe:**

```python
def make_heterogeneous_list():
    n = 3
    x = n * [0]
    i = 0
    x[i] += 1.5
    return x

def main():
    y = make_heterogeneous_list()
    print(y)

if __name__ == '__main__':
    main()
```

Six lines, zero relation to `tictactoe.py`. This alone reproduces
the identical `assert(!"runtime error: list element type
mismatch")`. Real Python's list is heterogeneous by design —
`n * [0]` then `x[0] += 1.5` legitimately produces `[1.5, 0, 0]`,
one `float` element among `int` elements — but pyc represents a
list's backing store as one concrete, unboxed C array type. A list
literal that starts homogeneous (`n * [0]`, all `int64`) and is
later mutated in place with a genuinely different scalar *kind*
(`float64`, via `+=`) can't be represented without boxing: unlike a
struct field (where 077/034/035's `num_kind`-scalar-tolerance rule
already lets two different scalar kinds share one field via a cast),
a list/array needs one element size and layout for *every* slot, so
FA has to fall back to a non-scalar/pointer (`_CG_void`) element
representation for the array itself — which is exactly what trips
`P_prim_set_index_object`'s (correct, working-as-designed) guard on
the next write.

**Why the crash surfaces via `set::add` → `list::append`, not
directly at the `scores[...] += ...` site that actually causes it:**
`tictactoe.py`'s `doRow` does exactly this — `scores` starts as
`[self.edge * [0] for i in range(self.edge)]` (all-`int64`,
`self.edge` a runtime variable so it's the dynamic/general list
representation, not a small compile-time-sized tuple-list), then
`scores[rown][coln] += 15 * sig(...)` (or the sibling `else` branch,
`+= 15 * fields.count(...) / float(self.edge)`) stores a genuine
`float64` into it. Confirmed both branches independently suffice
(removing either alone still crashes; removing both makes the crash
disappear even with `doRow`/`makeAImove` otherwise unchanged and
still called). Confirmed the converse too: pre-typing `scores` as
uniformly `float64` from construction
(`[[0.0 for j in range(self.edge)] for i in range(self.edge)]`
instead of `[self.edge * [0] ...]` + `+=`) removes the crash with
*everything else in the file, including every `set(...)` call,
unchanged*. `doRow` never calls `.append()` and never touches `set`
at all — but pyc's list-element-type inference for the general
dynamic-list representation isn't scoped tightly per allocation
site; `scores`'s rows and `set`'s `_items` field (itself grown via
`self._items = self._items.append(item)` in `08_set.py`) end up
sharing enough of that inference that `scores`'s genuine
int/float heterogeneity degrades `_items`'s element type to
`_CG_void` too, even though no float value is ever actually stored
into `_items`. Whichever list-write happens to execute first at
runtime is what's observed to assert — in `tictactoe.py`'s actual
run it's `set(row)`'s construction inside `isvictory()`, well before
`doRow` ever runs, which is why the crash trace pointed at
`set::add`/`list::append` and looked `set`-specific. It isn't: `set`
is not the source, just the first victim in this program's
particular execution order.

This is the same class of gap [018](018-dict-mixed-key-types-boxing-failure.md)
/ [ifa/030](../ifa/issues/030-DISPATCH-polymorphic-dispatch-fat-pointers.md)
already track (no boxed/tagged representation for a genuinely
heterogeneous scalar union) and the same *category* of finding as
[ifa/071](../ifa/issues/071-FA-chess-accumulated-union-notype-cascade.md)
chess.py's dig (an unrelated site's union reaching into a shared
structure) — but unlike 071, this one **does** reduce to a small,
fully general, non-program-specific minimal repro; it isn't an
"accumulated churn, no single root cause" situation. Not fixed here:
the durable fix is the same boxed/tagged `scalar` representation
018/030 already call for; a narrower option (silently widening
`scores`'s whole array to `float64`, including its originally-`int`
slots) would change observable output (`0` → `0.0` in `repr`/`print`)
and was rejected for the same reason issue 035's own guard exists —
prefer a loud runtime assert over a silently wrong value.

### The `webserver.py` regression: root-caused and fixed, 2026-08-06

A first attempt removed `__set_iter__`/`__dict_iter__`/
`__dict_items_iter__`'s class-body defaults, mirroring
[076](../ifa/issues/closed/076-mutation-driven-receiver-divergence-not-cloned.md)'s
exact `dict`/`set` fix — these three classes still had that exact
shape, flagged at the time as "not surveyed" in
[078](../ifa/issues/closed/078-class-body-default-plus-init-override-permanently-unions.md).
That **did** additionally fix `shedskin_examples/loop/loop.py` (FAIL
→ `COMPILED_C`) but did **not** fix `tictactoe.py`'s runtime crash,
and **regressed `shedskin_examples/webserver/webserver.py`** (fixed
by [032](closed/032-dict-view-membership-missing-contains.md),
earlier this session) back to a hard `_CG_str_eq` compile error.
Reverted at the time rather than ship a regression.

**Root cause, found by direct reduction of `webserver.py` itself:**
076's mechanism doesn't apply here the way it does to `dict`/`set`
themselves. `dict`/`set`'s class-body defaults were *spurious* —
`__init__` always overwrote them before any instance was observable,
so the class-body write was provably dead weight, and removing it
was a strict improvement. `__dict_iter__`/`__dict_items_iter__`/
`__set_iter__` are different: they're **shared program-wide** — every
`dict.keys()`/`.values()`/`.items()` call (and every `set` iteration)
constructs one, so their `_keys`/`_vals`/`_items` fields are
inherently the union of *every calling dict/set's* key/value/element
type, not a same-instance artifact. `webserver.py` has exactly two
such callers with genuinely different key types: `self.mapSocks.keys()`
(int-keyed, socket file descriptors) and `headers.keys()`/
`responseParams.keys()` (str-keyed). With the class-body defaults
*present*, that cross-instance union happened to land on a
salvage-friendly `_CG_any` representation everywhere it was used;
removing them changed the union's shape into something the
`_CG_str_eq` call site couldn't cast into — an accidental, not
principled, interaction. Confirmed via direct reduction: a two-line
repro (`{1: "a"}.keys()` and `{"x": 1}.keys()` both iterated in the
same program) reproduces the identical `_CG_str_eq`/`_CG_any` error
independent of `webserver.py` or even classes at all.

**Fix:** `__list_iter__` (`__pyc__/04_sequence.py`) and `range`
(`__pyc__/05_builtins.py`) already solve this *exact* class of
problem — shared, program-wide iterator classes whose fields would
otherwise union every caller's element type — via
`__pyc_clone_constants__` on the constructor parameter
([ifa/issues/045](../ifa/issues/closed/045-receiver-cs-method-cloning.md)):
it puts the class on the `clone_methods_per_cs` track in
`gen_class_pyda`, giving each *creating contour* its own iterator
CreationSet, with methods split per receiver CS too. Applied the same
lever to `__dict_iter__`/`__dict_items_iter__`/`__set_iter__`'s
`__init__` (`self._keys = __pyc_clone_constants__(keys)` etc.),
leaving their class-body defaults untouched (reverting to the
original, pre-076-style shape — correct here, since unlike `dict`/
`set` themselves these classes' class-body defaults were never the
actual problem).

**Verified working, with one known, narrower, pre-existing
limitation.** `webserver.py` and the direct two-dict-key-type repro
both compile clean and run correctly *when the `.keys()` calls happen
inside a function or method* — matching `webserver.py`'s actual
structure (`WebServer.poll()`) and every corpus example's typical
shape. The `clone_methods_per_cs` per-receiver-CS split this fix
relies on needs a per-call-site contour to split *by*; bare top-level
(`__main__`) code doesn't get the same per-invocation specialization
ordinary function bodies do, so the identical `.keys()` calls written
directly at module level (not inside any `def`) still reproduce the
original error. Confirmed this is **pre-existing, not introduced or
worsened** by this fix — the same module-level repro fails identically
against the baseline *without* this change too. Not investigated
further; `tests/dict_iter_cross_instance_keytype.py` is deliberately
wrapped in a function to test the case this fix actually addresses.

This did **not** fix `tictactoe.py`'s runtime crash (identical assert,
identical contour before and after) — that remains the deeper
`set`-element-union gap described above, unrelated to the iterator
classes' own field-union mechanism.

## Verification

- `ifa --test`: 58/58.
- `tests/list_element_type_mismatch_salvage.py` (new): ordinary,
  uniformly-typed list/tuple-list mutation through both
  `P_prim_set_index_object` branches, confirming that guard doesn't
  disturb normal usage. Compiles with zero warnings, output matches
  `python3` exactly.
- `tests/dict_iter_cross_instance_keytype.py` (new): an int-keyed and
  a str-keyed dict both calling `.keys()`/`.items()`/`in` inside a
  function — the exact `webserver.py` regression shape. Compiles with
  zero warnings, output matches `python3` exactly.
- `tictactoe.py`: compiles with **zero** warnings on both backends
  (was a hard compile error); does not yet run to completion (the
  separate, still-open `set`-element-union gap above).
- `webserver.py`: compiles with **zero** warnings on both backends and
  runs correctly end-to-end (started the compiled binary, `curl`'d
  it, got the correct response) — confirmed *with* the
  `__pyc_clone_constants__` fix applied, i.e. the regression a first
  attempt caused is closed.
- `test_pyc.py`, C and LLVM backends, `PYC_CSM` unset: 244/11/0/4
  both (242 baseline + 2 new tests, 0 regressions).
- `test_pyc.py`, C and LLVM backends, `PYC_CSM=2`: 240/11/4/4 both,
  same 4 pre-existing failures.
- `shedskin_sweep.sh`, both `PYC_CSM` settings: byte-identical to the
  `P_prim_set_index_object`-only baseline (diffed directly against
  saved `results.tsv`) — the `__pyc_clone_constants__` fix is
  corpus-sweep-neutral (neither a new win nor a new loss there; its
  value is confirmed via the direct `webserver.py` compile+run check
  and the new regression test, not the sweep).

## What this unblocks

Any program where a list's element type ends up genuinely or
speculatively pointer/scalar-mismatched against a stored value no
longer hits a hard build failure — matches the established
compile-clean-but-may-runtime-assert convention (issue 056) already
applied at every other salvage-reachable call site this investigation
has covered. Any program (not just `webserver.py`) with two or more
dicts/sets of genuinely different key/value/element types calling
`.keys()`/`.values()`/`.items()`/iterating from inside a function or
method no longer risks the same cross-instance union — this was a
general gap, not `webserver.py`-specific, just first found there.
Doesn't fix the underlying `set`-element-union precision gap
(`tictactoe.py`'s remaining runtime crash) or the narrower,
pre-existing module-level-code limitation described above — both
remain open for whoever picks them up next.

### Another confirmed instance, 2026-08-08: mixed-type tuple printing, and a new LLVM divergence

[issues/025](025-shedskin-examples-coverage.md) separately
noted (2026-07-16) that `print()`/`str()` of a tuple with genuinely
mixed element types — `(1, None)` — aborts at runtime
("matching function not found"), framed there as "distinct from the
general tagged-dispatch problem in issue 030." Re-verified 2026-08-08
while auditing that doc's own TODO list: still reproduces identically
on the C backend, and it's the *same* family this issue's "Root cause"
section already describes, not a distinct gap — traced via the
generated C to `__pyc_any_type__::__repr__(_CG_int64 a1)` (the generic
boxed-value repr dispatcher every corpus program shares) failing to
match a candidate for the tuple's `int64` element, i.e. exactly "no
boxed/tagged representation for a genuinely heterogeneous scalar
union" playing out at a tuple-print call site instead of a list-write
one. The 025 doc's "distinct from issue 030" framing predates this
issue's own later (2026-08-06) explicit grouping of 018/030 into the
same family above; treat this tuple-printing instance as another
confirmed member of that family, not a separate gap.

**New finding: the LLVM backend does not crash here at all — it
silently produces wrong output.** `print((1, None))` compiles and runs
clean on `-b` (exit 0), but prints `(, )` instead of `(1, None)` —
the tuple's fields print as empty strings rather than triggering any
guard. Matches the same "C backend fails loud, LLVM backend fails
silent" divergence already documented for
[061](../ifa/issues/061-CGEN-multi-tuple-list-null-element-type.md)'s
list-of-tuples case — worth keeping in mind if/when this family's
underlying boxed/tagged-scalar gap is eventually fixed: the fix needs
to give LLVM a genuine salvage guard here too, not just close the C
side's `assert`, since LLVM's current behavior is arguably worse (no
diagnostic at all, not even a crash).
