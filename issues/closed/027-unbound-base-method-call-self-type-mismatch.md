# Issue 027: Explicit unbound base-class method call (`Base.method(self)`) fails a type check

**Status:** **closed 2026-07-10** — fixed via `Sym::aspect` (the
`super()` static-dispatch mechanism), landed together with
`@staticmethod`/`@classmethod` support. `Base.method(recv, ...)` now
lowers to a period dispatch whose receiver flows through a temp that
masquerades as `Base` for pattern matching (`obj->aspect = Base`)
while keeping its concrete type — exactly `super(Base2, recv)`'s
shape, minus the superclass hop (`fixup_aspect` now only hops the
Syms `super`'s lowering registers in `super_aspect_syms`; qualified-
call aspects are final). Both the subtype reproducer and the
exact-match companion below pass on BOTH backends, byte-identical
to CPython; regression test `tests/unbound_base_call.py`.

The 2026-07-09 analysis below (kept for history) had concluded a
same-session fix wasn't reasonable — that conclusion was based on
the ATTEMPTED approach (bare-Fun direct call), whose FA segfault
diagnosis remains correct and is why the ASPECT route (which never
detaches the call from the closure/dispatch machinery) is the right
fix. Note also: the "worse crash on the exact-match case" that
attempt exhibited was later found to be partially confounded by a
stale-object build (sym.h bitfield layout change without
`make clean` — the project's known dep-tracking gap); the bare-Fun
approach was still wrong (`make_AVar` display-chain reasoning holds),
but crash symptoms from that session should be read with that
caveat.

Original status text follows.

**Status (superseded):** open, root-caused 2026-07-09, **not reasonable to fix in
a single session** — see "Attempted fix and why it's harder than it
looks" below. The obvious frontend patch (resolve `ClassName.method`
directly to the underlying Fun and call it plainly, bypassing the
buggy bound-closure path) DOES fix the originally-reported symptom,
but immediately trades it for a worse one: a compiler SEGFAULT in
FA's lexical-nesting resolution (`make_AVar`, `ifa/analysis/fa.cc:210`),
including on the previously non-crashing exact-type-match case. A
real fix needs FA-level closure-construction work (see below), not
just a frontend lowering change. Reverted; no code changes landed.
**Affects:** frontend call-argument type checking for unbound
method calls (`ClassName.method(instance, ...)` — the pattern
commonly used instead of `super().method(...)` to call a SPECIFIC
base's implementation, especially under multiple inheritance where
each base's `__init__` must be invoked explicitly rather than
relying on MRO chaining); ALSO `ifa/analysis/fa.cc`'s
`make_AVar`/nesting-depth resolution and `make_closure_var` (closure
value construction happens at FA-analysis time, not in the
frontend — see below).
**Related:** found while stress-testing
[closed/010-multiple-inheritance-unrelated-bases.md](010-multiple-inheritance-unrelated-bases.md)
(NOT part of that issue's scope — this reproduces under plain
single inheritance, no multiple inheritance needed at all).

## Symptom

```python
class A:
    def __init__(self):
        self.x = 1

class C(A):
    def __init__(self):
        A.__init__(self)
        self.z = 3

c = C()
print(c.x)
print(c.z)
```

CPython prints `1` then `3`. Pyc fails to compile:

```
unbound_call.py:7: illegal call argument type 'self' illegal: C
  called from unbound_call.py:5
unbound_call.py:7: illegal call argument type expression illegal: closure
  called from unbound_call.py:5
unbound_call.py:11: illegal call argument type expression illegal:
unbound_call.py:11: expression has no type
```

(Runtime, if forced past the diagnostics: `assert(!"runtime error:
getter not resolved")`.)

Line 7 is `A.__init__(self)`. The diagnostic says the argument typed
`self` (which is `C`, since we're inside `C.__init__`) is "illegal"
for a call that presumably expects an `A`-shaped receiver — i.e.
pyc's call-argument type check for `A.__init__` wants exactly an
`A` instance and rejects a `C` instance even though `C` is a
subtype of `A` (structurally and nominally, via `inherits_add`).

## Why (confirmed by tracing, 2026-07-09)

Dumped the IF1 IR (`pyc -v -v -v`) for the reproducer. `A.__init__`
is stored, like every method, as a prototype DATA FIELD (`A [ ...
__init__:__init__ ]`) whose value is a bound-closure record
`( closure [ __init__:__init__  4893:A ] )` — i.e. `[Fun=__init__,
receiver=A's prototype instance (Sym id 4893)]`. `python_ifa_build_if1.cc`'s
`PY_power` handler treats `A.__init__` exactly like `instance.method`:
it emits a period-dispatch GETTER send (`sym_operator, obj, sym_period,
pending_sym, t`) with `obj = cur_val->self` — the CLASS's own
prototype instance (`cur_val->self` is set ONLY on a class's own Sym,
never on an instance Var, by `gen_class_pyda`'s
`cls->self = new_global(...)`) — producing a closure `t` PERMANENTLY
BOUND to `A`'s prototype. The subsequent call then does
`send1(); add_send_arg(t); add_send_arg(self)` — i.e. it invokes the
ALREADY-BOUND closure `t` with the explicit `self` STACKED ON AS AN
EXTRA ARGUMENT, rather than using `self` AS the receiver. Confirmed via
the dump: BOTH `self:4917 (C)` and `expr:4958 (closure bound to A)`
are flagged "illegal call argument type" for the SAME send — the
send has two args where the callee (a bound `__init__` closure)
expects zero extra ones.

This also explains why the EXACT-type-match case (`A.__init__(a)`
where `a: A`, no subclassing) does NOT hard-fail today: the type
mismatch diagnostics still fire (same bogus send shape), but they're
apparently narrowed away or otherwise non-fatal for this specific
combination, and the program happens to produce the right runtime
value anyway. That's not evidence the mechanism is correct — it's
evidence the bug's exact failure mode is receiver-type-sensitive in a
way that isn't fully understood either.

## Attempted fix and why it's harder than it looks

The obvious fix: when `cur_val->self` is set (i.e. `X` in `X.attr` is
literally a class reference, not an instance) and `attr` names an
actual method (`cur_val->has[]` contains a field whose `->alias` is
an `is_fun` Sym — the same has[]-as-prototype-field-with-alias
pattern `gen_class_pyda`'s own inherited-field copy loop already
relies on), resolve `cur_val` DIRECTLY to that raw Fun (`cur_val =
h->alias; continue;` — mirroring the existing `is_module` special
case a few lines above), bypassing the period-dispatch getter
entirely. The subsequent `PY_call` trailer then treats it as an
ordinary direct function call with the source's arguments
(`self`, ...) passed positionally, unmodified — no bound receiver
to conflict with.

Implemented and tested: it DOES fix the reported symptom (the
subclass reproducer above compiles and runs correctly, printing `1`
then `3` on both backends). But it immediately regresses the
previously-not-crashing exact-match case (`A.__init__(a)`, `a: A`,
no subclassing) from "wrong-looking diagnostics but correct output"
to **a SIGSEGV inside the compiler itself**:

Reproduced under gdb (`gdb -batch -nx -ex "set debuginfod enabled
off" -ex run -ex "bt 20"`), full backtrace:

```
Program received signal SIGSEGV, Segmentation fault.
0x00005555556eb545 in make_AVar (v=0x7fffe96313c0, es=0x7fffeaad2400) at analysis/fa.cc:210
210	      return unique_AVar(v, es->display[v->sym->nesting_depth - 1]);
#0  0x00005555556eb545 in make_AVar (v=0x7fffe96313c0, es=0x7fffeaad2400) at analysis/fa.cc:210
#1  0x00005555556fbe59 in add_es_constraints (es=0x7fffeaad2400) at analysis/fa.cc:2485
#2  0x00005555556f863d in analyze_edge (e_arg=0x7fffe9772e60) at analysis/fa.cc:2575
#3  0x00005555556f1231 in analyze_to_convergence () at analysis/fa.cc:4822
#4  0x00005555556f0ae4 in FA::analyze (this=0x7fffea11a540, top=0x7fffe9640800) at analysis/fa.cc:4851
#5  0x00005555556a2fe2 in ifa_analyze (fn=0x7fffeecf0ff0 "exact_match.py") at ifa.cc:47
#6  0x0000555555675fd8 in compile (fn=0x7fffeecf0ff0 "exact_match.py") at pyc.cc:99
#7  0x00005555556765e8 in main (argc=4, argv=0x7fffffffd398) at pyc.cc:226
```

`v`'s Sym is the method Fun's formal receiver flowing through
`add_es_constraints`'s constraint setup for the freshly-created
EntrySet (`es`) at the bare-call site — i.e. the crash fires while
FA is trying to set up constraints for the very call my patch
introduced, confirming the mechanism described above rather than an
unrelated pre-existing fault.

Root cause: a method Fun's `nesting_depth` reflects its LEXICAL
definition context (being written inside `class A:`'s body). FA's
`make_AVar` (`ifa/analysis/fa.cc:207-213`) resolves a variable
belonging to an OUTER lexical scope by indexing
`es->display[nesting_depth - 1]` — the calling EntrySet's display
chain, populated for whatever scopes the CALLER is lexically nested
inside. A bare Fun value invoked directly (as my patch does) is
called from a context that has no reason to have `A`'s class-body
scope anywhere in ITS display chain (the caller isn't lexically
nested inside `class A:` at all — it's a sibling class `C`, or plain
module-level code) — so the index is invalid and FA segfaults.

The normal, WORKING call paths (`instance.method()`, `__new__`'s
own construction of bound method-slots) never hit this because they
ALWAYS go through closure construction, which happens not in the
frontend but at FA-ANALYSIS time (`ifa/analysis/fa.cc`'s
`make_closure_var`, driven by `P_prim_period` sends — confirmed by
grep, not the frontend's `if1_closure`, which is a different thing:
it *defines* a Fun's body, not a call-site closure *value*).
Whatever machinery packages a Fun together with its lexical context
into a valid closure value evidently also handles the display-chain
threading correctly; a bare Fun reference constructed by skipping
that machinery does not carry the same guarantee, and FA has no
fallback for it.

**The real fix therefore needs one of:**

1. A way to construct a closure value at FA-analysis time that's
   STATICALLY bound to `A`'s own `__init__` (not dynamically
   classtag-dispatched, since `A.__init__(self)` must call `A`'s
   version specifically, ignoring whatever override `self`'s actual
   dynamic type may have) but with the receiver field set to the
   CALLER-SUPPLIED explicit argument rather than the class
   prototype. This likely means a new or modified primitive send
   recognized by `make_closure_var`/`add_es_constraints`, distinct
   from period dispatch's dynamic classtag lookup.
2. Making bare Fun-value calls (my attempted frontend-only patch)
   safe in FA by giving `make_AVar` a fallback for a Fun invoked
   outside its constructing closure's context — riskier, since it's
   not clear what the CORRECT display-chain entries would even be
   for such a call (the caller has no lexical relationship to `A`'s
   scope to draw them from).

Both require depth in FA's closure/dispatch machinery
(`ifa/analysis/fa.cc` around `make_closure_var`, `add_es_constraints`,
`make_AVar`) that this investigation didn't need to reach for issues
003/010/026 (all of which were EITHER already fixed by prior work,
or confined to codegen-level dispatch-branch construction). This one
is a genuine FA-analysis-level gap.

## Reproducer

```python
class A:
    def __init__(self):
        self.x = 1

class C(A):
    def __init__(self):
        A.__init__(self)
        self.z = 3

c = C()
print(c.x)
print(c.z)
```

Expected (CPython): `1`, `3`. Currently: compile-time diagnostics,
runtime `getter not resolved` assert if forced past them.

Companion reproducer — the exact-match case, currently limping
through with bogus (non-fatal) diagnostics rather than failing
outright; any real fix must keep this working too, not just the
subtype case above:

```python
class A:
    def __init__(self):
        self.x = 1

a = A()
A.__init__(a)
print(a.x)
```

Expected (CPython): `1`. Currently: prints `1` (rc=0), but emits
`illegal call argument type` diagnostics for the same underlying
reason as the subtype case.

## What's needed for a real fix

1. Understand `make_closure_var` / `add_es_constraints` in
   `ifa/analysis/fa.cc` well enough to add a statically-bound
   closure-construction path (see options 1/2 above) — this is the
   actual remaining work; the frontend lowering site is already
   identified precisely (`python_ifa_build_if1.cc`'s `PY_power`
   handler, the `PY_attribute` trailer branch) and a draft patch
   exists in this issue's history (reverted, not committed) as a
   starting point for the frontend HALF of the fix, once the FA side
   has somewhere safe to route to.
2. Verify against BOTH the subtype case (this issue's reproducer)
   AND the exact-match case (`A.__init__(a)`, `a: A`) — the fix must
   not regress the latter into a crash the way the naive attempt did.
3. Verify both backends; add a regression test (the reproducer
   above, `.exec.check` = `1\n3\n`), plus the exact-match variant.

## What this unblocks

- The idiomatic multiple-inheritance constructor pattern (each
  base's `__init__` called explicitly and unconditionally, rather
  than relying on `super()`/MRO chaining) — arguably the MOST common
  real-world use of this call form, since `super().__init__()` alone
  doesn't reliably reach every base in a non-linear multi-inheritance
  hierarchy without careful cooperative-`super()` design.
- Any code calling a specific base's method by name to bypass an
  overriding subclass's own version (a standard, deliberate Python
  idiom, not just a workaround).
