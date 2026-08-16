# Issue 018: Using `dict`/`set` with two different element types in one program fails to compile

**Status:** open — but **the container half is FIXED** (verified
2026-08-16) and only the bare-scalar half remains. Retested every shape
this doc describes:

| shape | today |
|---|---|
| two dicts, `int` vs `str` keys (the headline repro) | **passes** |
| two sets, `int` vs `str` elements | **passes** |
| mixed *value* types across two dicts | **passes** |
| object keys alongside `int` keys | **passes** |
| three key types (`int`/`str`/`float`) in one program | **passes** |
| **bare branch-merged scalar** (`x = 5` / `x = "hi"`, then `x + ...`) | **still fails** |

The five passing shapes are pinned by `tests/dict_mixed_key_types.py`.
The failure is pinned by `tests/branch_merged_scalar_union.py`
(`.known_issue`): 8 warnings, compiles, then the binary aborts with
`matching function not found` where CPython prints `hi world`.

**Correction (2026-08-16): this residue is NOT
[048](048-none-int-field-pair-runtime-abort.md)**, despite both aborting
with `matching function not found` on the C backend. They share a symptom
string, not a cause, and the backends separate them cleanly:

| | C backend | LLVM backend |
|---|---|---|
| 048 (`None\|int` field pair) | aborts | **correct** — a `cg.cc` bug |
| this (`int\|str` branch merge) | 8 warnings, aborts | **also fails** — `LLVM module verification failed: call ptr @_CG_strcat(ptr %0, i64 10)` |

Failing on *both* emitters means the defect is upstream of codegen: FA
genuinely does not resolve the union, exactly as the section below
describes (it clones `str.__add__` for the union's `str` member with the
call site's literal `10` baked in). That is this issue's own mechanism,
and no other open issue covers it — 048 is codegen,
[075](../ifa/issues/075-FA-element-cs-method-split-idempotent-plan.md) is
the container-method-per-element-CS plan, and
[030](../ifa/issues/030-DISPATCH-polymorphic-dispatch-fat-pointers.md) is
classtag dispatch for object receivers, which this doc already notes does
not cover a raw `int`/`str` scalar union.

**So 018 stays open on its own merits**, re-scoped to the bare-scalar
union. The dict/set half is done.

Note also that the `sizeof_element of non-container` guard in `cg.cc`
still cites this issue and still fires: it blocked the C-helper form of
[050](050-pyc-string-builders-are-quadratic.md)'s fix when a trajectory
change let `list.__add__` be specialised against a `bytes` receiver. So
the *mechanism* named here is alive even though the dict/set symptoms are
gone; [075](../ifa/issues/075-FA-element-cs-method-split-idempotent-plan.md)
remains the fix vehicle for that half.

**Original status:** open.
**Affects:** `dict`'s shared internal comparison logic
(`__pyc__/07_dict.py`'s `_keys[i] == key` checks in `__getitem__`/
`__setitem__`/`get`/`__contains__`) and `set`'s equivalent
(`__pyc__/08_set.py`'s `_items[i] == item` in `__contains__`/`add`/
`discard`); FA's `BOXING` type-violation path
(`ifa/analysis/fa.cc:2864-2869`, `ATypeViolation_kind::BOXING`).
**Related:** discovered while writing regression tests for issue 009
(dict comprehensions) — confirmed unrelated to comprehensions or to
that fix; reproduces identically with two plain flat dict literals
and zero comprehension code involved. Also affects `set` (issue
008's new class) identically — same shared-linear-scan-comparison
shape, same failure. Not a literal duplicate of any single
`ifa/issues/` file, but the same underlying gap as
[ifa/issues/063](../ifa/issues/closed/063-no-type-bucket-triage.md) (diagnosis)
/ [ifa/issues/075](../ifa/issues/075-FA-element-cs-method-split-idempotent-plan.md)
(concrete build plan) — pyc's shared `list`/`dict` container methods
aren't cloned per element/key-CS, so a program with two
differently-keyed dict instances gets one merged AVar for `key`
across both; 063 states this almost verbatim ("the single
`dict.__getitem__` contour reads one element AVar that is the union
of [multiple element types]") for the object-key/NOTYPE flavor, and
this issue is the basic-scalar-key/BOXING flavor of the identical
mechanism — 075's CSM (clone container methods per element-CS)
should fix both together. Also already informally paired with
[ifa/issues/030](../ifa/issues/030-DISPATCH-polymorphic-dispatch-fat-pointers.md)
elsewhere in the tree (`ifa/issues/073`, `ifa/issues/README.md` both
say "the 018/030 heterogeneous-union boxing family") — 030's classtag
dispatch is for object/class receivers though, so it doesn't directly
cover this issue's raw `int`/`str` scalar union; it's the sibling
bucket, not the fix vehicle. Confirmed via code: `__pyc__/04_sequence.py`
(list) already opts into `__pyc_clone_constants__`/`clone_methods_per_cs`
(closed [ifa/issues/045](../ifa/issues/closed/045-receiver-cs-method-cloning.md));
`__pyc__/07_dict.py` has none — but 045's mechanism is
receiver-*identity*-based (same type, different instance, e.g.
empty vs non-empty list), not element-*type*-based, so opting dict
into it alone would not fix this issue even if done.

## Symptom

```python
squares = {1: 1, 2: 4, 3: 9}
print(squares[3])
words = {"a": 1, "b": 2}
print(words["b"])
```

```
warning: illegal primitive argument type 'x' illegal: str
  called from __pyc__:627
__pyc__:332: expression has mixed basic types:( int64 str )
  called from __pyc__:620
...
squares[3]: ISO C++ forbids comparison between pointer and integer [-fpermissive]
  t1 = _CG_prim_equal(t2, _CG_Symbol(2816, "=="), _CG_String("a"));
```

...and the C compile fails outright. Each dict works fine
*individually* — it's specifically having an `int`-keyed dict and a
`str`-keyed dict both live in the same program that breaks. The
identical shape reproduces with `set`:

```python
nums = {1, 2, 3}
print(len(nums))
strs = {"a", "b"}
print(len(strs))
```

(same `"has mixed basic types:( int64 str )"` failure, from `set`'s
`__contains__`/`add` this time.)

## Root cause (partially traced)

`dict.__getitem__`/`__setitem__`/`get`/`__contains__` all do
`self._keys[i] == key` — one shared method body per dict operation,
not cloned per key type the way e.g. `list`'s element type
specializes per instantiation. When the program also constructs a
dict with a *different* concrete key type, FA ends up flowing both
`int64` and `str` values into the same `key`/`_keys[i]` AVar during
specialization of these shared methods, and flags a `BOXING`
violation (`ATypeViolation_kind::BOXING`, "has mixed basic types") —
`int64` and `str` don't share a representable layout without boxing,
and pyc apparently doesn't (or can't, for `dict`'s specific call
shape) box here, so it falls through to the C backend with an
unresolved/mismatched type, producing invalid generated C.

Not yet traced further — unclear whether the fix is "FA should
box these" (making it work, at some cost) or "`dict`'s methods need
per-key-type specialization" (mirroring how other generic containers
apparently already get this right, e.g. `list` holding different
element types across separate instances works fine per the existing
test suite and this session's issue 017 stress-testing).

**Update:** the second option is the one already being pursued, one
level down — see the "Related" cross-references above. `ifa/issues/075`'s
CSM (element-CS container-method separation) is designed for exactly
this shape (a shared `list`/`dict` method whose receiver is a union of
same-container-type CreationSets with divergent element types) and
should resolve this issue's BOXING failure as a side effect once it
lands, without this issue needing its own separate fix.

## Scope confirmed broader (2026-08-03): a single heterogeneous `list` literal hits this too, not just cross-instance dict/set

Found while investigating an apparent `isinstance(x, list)`-vs-union
bug (`ifa/issues/025`), which turned out to be a downstream symptom of
this exact issue reached through a different, previously-untested
shape. This issue's own filing and repro are about **two separate,
internally-consistent** containers (an all-`int`-keyed dict and an
all-`str`-keyed dict) sharing a method across instances. A **single
`list` literal whose own elements already mix basic types** hits the
identical BOXING violation and crash, with no cross-instance sharing
involved at all:

```python
lst = [1, "hello"]
for v in lst:
    print(v)
```

Compiles with the same `warning: 'v' has mixed basic types:( int64
str )`, and **crashes at runtime**: `assert(!"runtime error: matching
function not found")` — worse than this issue's own original repro
(a clean compile failure), since it compiles "successfully" (with
warnings) and only fails when actually run. Confirmed independent of
`isinstance`: `print(v)` alone triggers it; `len(lst)` (which never
touches an individual element) does not. Likely the harder of the two
shapes for `075`'s CSM to resolve on its own — CSM specializes a
*shared method* per receiver element-CS, which presumes each
*instance* is internally monomorphic to begin with (matching this
issue's own original dict/set repro); a list literal whose elements
are already a mixed-basic-type union within one instance has no
consistent per-instance element-CS to specialize by in the first
place. Worth reconfirming against 075's design once that work is
picked up — flagging now so it isn't assumed covered "for free."

## Scope confirmed broader still (2026-08-11): no container needed at all — a bare branch-merged scalar hits the identical mechanism

Found while digging into [ifa/issues/025](../ifa/issues/025-FA-intra-function-union-narrowing.md)'s
"Case 1" (originally framed as an intra-function narrowing gap) —
then confirmed that **all three** of 025's originally-described cases
reduce to this same mechanism, not just Case 1 (see that issue for the
full account; summary here). Both prior shapes above involve a
*container* (two dict/set instances, or one heterogeneous list
literal). None of that is needed — a plain scalar variable merged
from two branches, with no container anywhere, hits the same
violation the moment it reaches an ordinary binary op:

```python
if cond:
    x = 5
else:
    x = "hi"
print(x + 10)
```

(`cond` must be genuinely runtime-varying — e.g. read from `sys.argv`
— or FA constant-folds the whole branch away before this code path is
ever exercised, same caveat as everywhere else in this codebase.)
Compiles with the same `warning: 'x' has mixed basic types:( int64
str )`, but this trigger shape lands on the **compile-error** side
rather than 018's list-literal runtime crash: FA clones `str.__add__`
(`__pyc__/01_str.py`: `return __pyc_operator__(self,
__pyc_symbol__("::"), x)`, untyped Python, a generic pass-through to
the `::`/`prim_strcat` table primitive) for the union's `str` member
with the call site's literal `10` (int64) baked in as the argument —
even though `prim_strcat` is declared `{STRING, STRING} → STRING`
(`ifa/if1/prim_data.cc:278`) and FA's own checker separately flags the
mismatch as a `PRIMITIVE_ARGUMENT` violation. In permissive mode
that's just a warning, and codegen emits the literal (invalid) call
anyway: `_CG_prim_strcat(t2, ..., 10)` — a hard C compile error
(`no matching function for call to '_CG_strcat'`) on the C backend,
an LLVM module-verifier failure (`call ptr @_CG_strcat(ptr %0, i64
10)`) on LLVM. Whether a given trigger lands on "compiles with a
warning, segfaults/asserts at runtime" (this issue's list-literal
case) or "doesn't compile at all" (this case) appears to depend only
on whether the *specific* invalid clone this mechanism generates
happens to produce C/IR that's still well-typed enough to build — not
on anything more principled.

This also answers a question [030](../ifa/issues/030-DISPATCH-polymorphic-dispatch-fat-pointers.md)
left implicit: classtag dispatch requires `cg_has_classtag`, which
requires `type_kind == Type_RECORD` — `int64` and `str` are never
`Type_RECORD`, so a call site whose receiver is a *raw scalar* union
has no discriminator available at all (no classtag, and no
address-identity fallback either, since that route is for closures/
plain functions, not values). That's the concrete mechanism behind
the "030's classtag dispatch... doesn't directly cover this issue's
raw `int`/`str` scalar union" note in this issue's own "Related"
section above — confirmed by code, not just inference, this session.

Checked the other two of `ifa/issues/025`'s originally-described
cases too, since the mechanism doesn't look specific to `+`/`str.__add__`:
a **bare `print(v)`** (no `isinstance`, no operator at all) on a
scalar union arriving via a *function return* (025's Case 2 shape:
`v = maybe(b)` where `maybe` returns `int` on one path and `str` on
the other) crashes identically (`assert(!"runtime error: matching
function not found")` — the dispatch for `__pyc_to_str__`/`__str__`
has nothing to resolve to, same as `dict`/`list`'s methods above).
025's Case 3 (a function whose own *return type* is a scalar union,
consumed by a caller) crashes the same way the moment the caller does
anything with the result. **All three of 025's cases are this
mechanism** — narrowing `x`'s (or `v`'s) type per-branch, even if it
worked perfectly, wouldn't be sufficient to fix any of them: the union
value itself has no coherent runtime representation for a generic
consumer (`+`, `print`, `isinstance`, ...) to dispatch on, independent
of whether the *use site* could in principle be proven type-safe by
narrowing. 025 has been updated to point here instead of describing
its own open narrowing mechanism for these three cases; the one thing
025's narrowing mechanism *does* genuinely fix — `is None` narrowing
on a `SomeClass | None` union — is unaffected by any of this, since
class instances (unlike raw scalars) already have a coherent runtime
representation (a pointer, optionally classtag-headed) and never trip
this issue's `BOXING` violation in the first place (`to_basic_type`
returns nullptr for classes and `None` — see 025's own file for that
mechanism).

## A more severe manifestation: shared container method specialized against a genuine non-container (2026-08-11)

Found compiling `shedskin_examples/rdb/rdb.py` after fixing
[issues/041](041-stdlib-shim-stubs-silently-wrong.md)'s `getopt`/`os`
stubs (unrelated to this issue directly — real `getopt`/`os.listdir`
made previously-dead-code-eliminated branches in `rdb.py` live for the
first time, and one of them hits this). Hard compile-time failure, not
a warning or runtime crash:

```
fail: ./__pyc__.py:986: internal: sizeof_element of non-container
type 'str' (in __add__) -- FA specialized a container method against
a scalar
```

(`ifa/codegen/cg.cc`'s `P_prim_sizeof_element` case — a container
method, e.g. `list.__add__`, needs to know its element's byte size to
emit the memcpy-style body; when the resolved type has no `element` at
all, the receiver isn't a container in the first place. Codegen fails
cleanly with a location instead of dereferencing null — this is *not*
new: the guard and its "see issues" pointer already existed, previously
citing 025 before its cases were redirected here.) A prior instance is
already recorded in that guard's own comment: score4's `list.__add__`
specialized with an `int64` right operand. This is the same mechanism
one step more severe — not just a *differently-typed scalar* reaching
a shared container method (018's core repro), but a **genuine
non-container** (`str`, no `element` field to even guess a size from)
reaching one. Same root cause as this issue's whole file: a shared
`list`/`dict`/etc. method isn't cloned per element/receiver-CS, so its
one shared contour can end up seeing a union that includes something
the method body's container-shaped codegen can't handle at all. Not
investigated further than confirming the mechanism match — no new
minimal repro isolated, `rdb.py` itself is the only current trigger
known.

## Related: ifa/issues/096 (closed/077's known remaining gap), found the same day

Also found via the same `getopt`/`os` fix, in
`shedskin_examples/msp_ss/msp_ss.py` — a *different* mechanism, not
this issue's, but recording the cross-reference here since it was
found alongside: several `__pyc_c_call__` sites (`_CG_fopen`,
`_CG_chr`, `_CG_str_to_int64_base`) fail with "no matching function...
cannot convert argument of incomplete type `_CG_any`". This is
[closed/077](closed/077-primitive-equality-codegen-missing-salvage-guard.md)'s
own documented, deliberately-unfixed remainder — that issue's final
design explicitly whitelists only the `str`-comparison family
(`_CG_str_eq` and siblings) for a salvage guard, and its own text says
"every other `__pyc_c_call__` site (`_CG_list_add`, `_CG_ord`,
`_CG_str_from_int`, `_CG_format_string`, ...) is completely unchecked
and unaffected" — `_CG_fopen`/`_CG_chr`/`_CG_str_to_int64_base` are
exactly such unchecked sites. Not a new bug, but a live, currently-
failing corpus program rather than only a theoretical remainder, so
it's tracked as its own open issue —
[ifa/issues/096](../ifa/issues/closed/096-extend-c-call-salvage-guard-past-str-comparisons.md)
— rather than folded into this issue or into reopening 077.

## Verification plan

1. The repro above compiles and both `squares[3]` (9) and
   `words["b"]` (2) print correctly.
2. A single program using `dict[int, ...]`, `dict[str, ...]`, and
   `dict[float, ...]` (three key types) together.
3. Existing single-key-type dict tests (`tests/dict_basic.py`,
   `tests/dict_methods.py`, `tests/dict_comprehension_basic.py`)
   continue to pass unchanged.
4. The bare-scalar repro above (needs a non-constant-foldable `cond`)
   compiles and prints `15` / `hi world`, matching CPython.

## What this unblocks

Realistic programs routinely use dicts keyed by different types in
different places (e.g. a `dict[str, int]` config alongside a
`dict[int, str]` lookup table) — this is a significant, easy-to-hit
limitation for anything beyond a single-key-type toy program. The
existing dict test suite (`tests/dict_basic.py`, `tests/dict_methods.py`)
happens to only ever use string keys, which is exactly why this had
gone unnoticed.
