# 047 — `write_c_prim` aborts the compiler on a nameless primitive destination

**Status:** open, 2026-08-15. Found while narrowing
[046](046-default-arg-omitted-differently-silently-wrong.md); unrelated to
it. **pyc aborts** (SIGABRT) rather than diagnosing.

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

## Verification plan

- The repro compiles (whatever it emits) instead of aborting.
- Full suite and shedskin sweep unchanged otherwise.

## What this unblocks

Nothing blocked today — no corpus program hits it. It is filed because an
`assert` in the compiler on ordinary Python is a poor failure mode, and
because it is a second witness to the untyped-contour condition that
[035](035-list-element-cast-salvage-guard-and-set-item-union.md) and
issue 025 both had to guard against elsewhere.
