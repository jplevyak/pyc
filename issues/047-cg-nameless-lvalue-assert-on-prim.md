# 047 — `write_c_prim` aborts the compiler on a nameless primitive destination

**Status:** FIXED 2026-08-19 (`c7468a91`). Originally filed 2026-08-15. Found while narrowing
[046](046-default-arg-omitted-differently-silently-wrong.md); unrelated to
it. **pyc aborts** (SIGABRT) rather than diagnosing. Repro landed as
`tests/method_setter_field_pair_compiler_abort.py` with a `.known_issue`
tag.

**Narrowed 2026-08-15:** the two setters must be **methods** — the same
program written with free functions (`def seta(v, x): v.a = x`) compiles
fine. `__slots__` is not required either; the test is the 20-line form.

## Symptom

```
pyc: codegen/cg.cc:389: int write_c_prim(FILE *, FA *, Fun *, PNode *):
     Assertion `cg_get_string(n->lvals[0])' failed.
```

on this 27-line program:

```python
class Clause:
    def why(self): return 7

class VarInfo:
    __slots__ = ['reason', 'reason_txt']
    def __init__(self):
        self.reason = None
        self.reason_txt = None

class Solver:
    def __init__(self):   self.v = VarInfo()
    def enq_r(self, reason):        self.v.reason = reason
    def enq_t(self, reason_txt):    self.v.reason_txt = reason_txt
    def run(self):
        self.enq_r(Clause())
        self.enq_t("learnt")
        c = self.v.reason
        if c: return c.why()
        return 0

print(Solver().run())
```

CPython prints `7`.

## Root cause (as far as traced)

`cg_get_string(n->lvals[0])` returning null means the primitive's
destination Var has no name — the same "dead/nameless destination"
condition that `P_prim_index_object`'s record branch already guards
explicitly (see `cg.cc`, the `cg_get_string(n->lvals[0])` check added for
issue 025's amaze/voronoi2 "no type" bucket, which skips the emission).
`write_c_prim` has no such guard and asserts instead.

So this is very likely the *same* class of unreached/untyped contour, just
reaching a primitive rather than a getter. The fix is presumably the same
shape — skip (or emit the established runtime-assert) rather than abort —
but the assert firing means it has never been exercised, so the right
behaviour for each primitive kind needs deciding rather than assuming.

## C backend only

`write_c_prim` is `cg.cc`, so this cannot fire under `PYC_FLAGS="-b"` —
the LLVM run reports the test as PASS. That is not evidence the condition
is absent from the LLVM path, only that the assert is not on it.

## Verification plan

- The repro compiles (whatever it emits) instead of aborting.
- Full suite and shedskin sweep unchanged otherwise.

## Fix

The predicted shape was right: skip the emission. A dead destination has
no name, the read is a pure struct field access, and with nothing to
assign it to there is nothing to emit — which is what
`P_prim_index_object`'s record branch already does on the identical
`cg_get_string(n->lvals[0])` condition.

## What this unblocked — the original estimate was wrong

This section used to read "Nothing blocked today — no corpus program
hits it." It was blocking **two**:

    corpus   67 compiled / 10 failed  ->  69 compiled / 8 failed
             softrender and tarsalzp now compile

Both still crash at runtime, so it is a compile-level gain only. The
estimate was wrong because the assert fires during codegen, after the
sweep's earlier failure modes; a program that failed FA for an
unrelated reason never reached it, and both of these only started
reaching codegen once other work (issues/113's package imports, for
tarsalzp) got them that far. **A "nothing depends on this" note is
worth re-testing whenever the phases ahead of it change.**
