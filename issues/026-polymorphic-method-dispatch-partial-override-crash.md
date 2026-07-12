# Issue 026: Polymorphic method dispatch crashes (C) / silently drops calls (LLVM) when not every union member overrides the method

**Status:** open.
**Affects:** method-call dispatch codegen — `ifa/codegen/cg.cc`
(`emit_send_call`'s classtag-branch construction, C backend) and
`ifa/codegen/cg_emit_llvm.cc` (same shape, LLVM backend).
**Related:** [ifa/issues/030-polymorphic-dispatch-fat-pointers.md](../ifa/issues/030-polymorphic-dispatch-fat-pointers.md)
(the classtag dispatch mechanism this bug lives in — "core
implemented," but its stated remaining scope doesn't mention this
gap); found while stress-testing
[closed/003-subclass-struct-layout-mismatch.md](closed/003-subclass-struct-layout-mismatch.md)
(unrelated to that issue's struct-layout root cause — this
reproduces with zero data fields involved, pure method dispatch).
**Likely second manifestation:**
[023-structural-pattern-matching.md](023-structural-pattern-matching.md)'s
"Known limitation" section — `case None:` combined with almost any
other `match`/`case` pattern in the same statement hits the
identical assertion (`Assertion '!"runtime error: matching function
not found"'`, same wording, same `cg.cc` fallback). Not confirmed
to be the SAME root cause, but the crash's shape (a class relying
on an inherited/generic implementation of some needed method, once
a second narrowing branch for the same subject follows) lines up
with this issue's own hypothesis closely enough that whoever
root-causes one should check the other's repro too before treating
them as separate bugs.

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

CPython prints `circle`, `shape`, `shape`. On pyc:

- **C backend**: compiles, then aborts at runtime:
  ```
  methodonly.py.c:379: _CG_nil_type _CG_f_107_18(): Assertion
    `!"runtime error: matching function not found"' failed.
  ```
  (`assert(!"runtime error: matching function not found");` emitted
  in place of the dispatch call — see `emit_send_call`'s classtag
  branch construction in `cg.cc`.)
- **v2 LLVM backend**: compiles AND runs to completion with exit
  code 0, but silently prints **nothing** — the `describe()` calls
  and their `print()`s are dropped entirely. This is the more
  dangerous variant: no crash, no diagnostic, just missing output.

Minimal repro needs only ONE class with no override at all in the
union (`Square`, above, or the base `Shape` instantiated directly);
every class DOES have a `describe` available via inheritance, so
this isn't a missing-method error — it's a dispatch-construction
gap specifically for classes that don't have their OWN override.

## Why (hypothesis, not yet root-caused)

`ifa/issues/030`'s classtag dispatch groups call candidates by
receiver class and reads each class's own stored method-pointer
slot (`obj->e<slot>`, set at `__new__` time to that class's
concrete implementation). The mechanism this session's nil|record
fix (`1ef66656`) extended assumes every candidate Fun's receiver
type can be resolved to a stored slot on SOME concrete class. A
class that doesn't override the method presumably doesn't get its
own slot-holding candidate branch constructed — the classtag
partitioning likely only builds one branch per class that has an
**own** `Fun` matching the call, and a class relying purely on the
inherited implementation has no such Fun in the classtag-scan
(`fun_val->sym->has` lookup keyed by method NAME on that formal's
OWN type), leaving it with no dispatch branch at all: `ok=false`
short-circuits the whole classtag construction (C backend: the
`assert` fallback fires) or the LLVM emitter's equivalent silently
returns without emitting a call.

Not verified with a debugger — this is what the code shape (traced
for the nil-receiver fix earlier this session) suggests, not a
confirmed root cause. Whoever picks this up should start with
`PYC_DBG_DISPATCH=1` (the env-gated dump added in commit `1ef66656`
at the C backend's unresolved-dispatch fallback in `cg.cc`) to see
exactly which candidates FA resolved and why none produced a
usable branch, then check whether `Square`/`Shape`'s `describe`
even gets its own stored method-pointer slot at `__new__` time when
it's purely inherited.

## Reproducer

`tests/`-shaped minimal case (not yet added as a test — this issue
is filed for triage, not landed with a fix):

```python
class Shape:
    def describe(self):
        return "shape"
class Circle(Shape):
    def describe(self):
        return "circle"
class Square(Shape):
    pass

items = [Circle(), Square(), Shape()]
for it in items:
    print(it.describe())
```

Expected (CPython): `circle`, `shape`, `shape`.

## What's needed for a real fix

1. Confirm the hypothesis above with `PYC_DBG_DISPATCH=1` and a
   debugger trace of classtag-branch construction for this repro.
2. Whichever class lacks its own override needs a dispatch branch
   that calls through to the INHERITED implementation's stored
   slot (or shares the base class's slot value, since `__new__`
   presumably already copies inherited method-value fields the
   same way it copies inherited data fields — see
   `gen_class_pyda`'s `cls->includes` copy loop, also relevant to
   issue 003's closed writeup).
3. Fix both backends; the LLVM silent-drop is the more urgent half
   since it produces wrong output with no diagnostic at all rather
   than a loud, debuggable crash.

## Verification plan

1. Reproduce on current `main` (confirmed 2026-07-09): C backend
   aborts, LLVM backend silently drops output.
2. Land the fix.
3. Add the repro above as a test (`tests/poly_dispatch_partial_override.py`
   or similar), `.exec.check` = `circle\nshape\nshape\n`.
4. Both backends pass; full suites stay green.

## What this unblocks

- Any polymorphic method-dispatch pattern where a subclass
  legitimately relies on an inherited implementation rather than
  overriding every dispatched method — the common case of
  "override only what differs," which is more common in real
  Python OO code than `ifa/issues/030`'s existing tests (where
  every concrete class in the tested unions happens to define its
  own override).
