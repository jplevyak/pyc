# 046 — two call sites omitting *different* defaulted parameters: the defaulted store is lost (silent wrong answer)

**Status:** open, 2026-08-15. Root-caused from `shedskin_examples/sat`'s
runtime failure. Minimal repro landed as
`tests/default_arg_omitted_differently.py`, tagged `.known_issue` (see
the README's "Marking a test as a known issue" — its check files hold the
CORRECT answer, so it turns green by itself when this is fixed). **This is a wrong-answer miscompile, not a
diagnostic** — pyc compiles cleanly except for one warning and then
prints a different number than CPython.

## Symptom

```python
class Clause:
    def why(self): return 7

class VarInfo:
    def __init__(self):
        self.reason = None
        self.reason_txt = None

class Solver:
    def __init__(self):        self.v = VarInfo()
    def enqueue(self, reason=None, reason_txt=None):
        self.v.reason = reason
        self.v.reason_txt = reason_txt
    def run(self):
        self.enqueue(reason=Clause())      # omits reason_txt
        self.enqueue(reason_txt="learnt")  # omits reason   <- DIFFERENT omission
        c = self.v.reason
        if c: return c.why()
        return 0

print(Solver().run())
```

CPython prints **0** — the second call resets `reason` to its default
`None`, so `c` is falsy. pyc prints **7**: the second call never wrote the
default, so `self.v.reason` still holds the first call's `Clause`.

The one diagnostic emitted is a *consequence*, not the bug:

```
warning: illegal call argument type 'c' illegal: str
```

i.e. `self.v.reason` has also acquired `str` from the other field.

## Narrowing (all measured)

| variant | result |
|---|---|
| as above | **miscompiles** (0 → 7) |
| without `__slots__` | **miscompiles** — not slots-related |
| both args passed explicitly (`enqueue(Clause(), None)`) | clean |
| both call sites omit the **same** parameter | clean |
| one call passes both, the other omits one | clean |
| only ever omitting the **second** parameter | clean |
| distinct non-`None` defaults | 1 warning; answer coincidentally matches |

**Trigger: two call sites of the same function that omit _different_
defaulted parameters.** Not `__slots__`, not `None`-as-the-default, and
not keyword syntax as such — `sameomit` uses the same keyword syntax and
is clean.

## Why it matters / what it broke

`shedskin_examples/sat` has exactly this shape:

```python
def enqueue(self, lit, reason=None, reason_txt=None):
    ...
    var_info.reason = reason
    var_info.reason_txt = reason_txt
```

with call sites passing `reason=<Clause>` at some points and
`reason_txt="considering"` / `reason_txt=reason` at others (sat.py:480,
496). The result is `cause = var_info.reason` typed `Clause ∪ str`
(sat.py:373/377 warn `illegal call argument type 'cause' illegal: str`),
and at run time sat dies with an unhandled `AssertionError` — one of its
many `assert`s fails because the solver's bookkeeping is wrong.

`sat` began compiling only after the 2026-08-15 divergence-guard fix
(ifa/issues/074), which is how this surfaced; the defect itself is
independent of and older than that change.

## Mechanism, from the generated C (2026-08-15)

**The title's framing is wrong and is kept only because it names the
trigger.** This is not a lost default *store*; it is a mis-resolved
instance-field *slot* inside one of the clones.

pyc emits two clones of `enqueue`, one per call shape:

```c
/* clone for enqueue(reason=Clause()) -- CORRECT */
_CG_f_10189_34(_CG_ps11035 a1, _CG_ps10142 a2) {
  t2 = ((_CG_ps11035)t1)->e14;          /* self.v      */
  ((_CG_ps11036)t2)->e13 = (_CG_void)t3; /* v.reason = Clause */
}

/* clone for enqueue(reason_txt="learnt") -- WRONG, both statements */
_CG_f_10189_40(_CG_ps11035 a1, _CG_string a2) {
  t2 = ((_CG_ps11035)t1)->e14;
  ((_CG_ps11036)t2)->e13 = (_CG_void)_CG_String_n("learnt",6);
}
```

`VarInfo` has exactly one data field, `e13 /* reason */` — `reason_txt`
is elided, which is **correct and normal**: the clean control variants
(`sameomit`, `positional`) also emit one field and produce the right
answer, because there the store to the elided field is simply dropped.

In the failing clone both statements are wrong:

1. `self.v.reason = reason` (with `reason` at its nil default) is
   **dropped** — so the field keeps the *previous* call's `Clause`. This
   alone is the wrong answer.
2. `self.v.reason_txt = reason_txt` is emitted **against `reason`'s
   slot** (`e13`), so `"learnt"` lands in `reason`. That is why
   `c.why()` then dispatches `Clause::why` on a string and returns 7.

So: **in a clone where a defaulted parameter is nil-typed, the nil store
is dropped and the next store is emitted against the dropped field's
slot.** The field-slot assignment (`AVar::ivar_offset`, written
post-convergence by clone) is the thing to look at — the offsets are
computed from the CreationSet's union view while the clone's emission
appears to shift when a field is elided.

## Ruled out (each measured, do not re-try)

`__slots__`; `None` as the default specifically (non-nil defaults still
fail); keyword syntax as such (`sameomit` uses it and is clean); one
attribute name being a *prefix* of the other (`reason`/`note` fails
identically); dead-field elimination as the cause (reading `reason_txt`
does not keep it alive and does not fix the answer); and parameter names
shadowing the attribute names (renaming the parameters to `r`/`t` changes
nothing).

## Also found while narrowing — now [048](048-none-int-field-pair-runtime-abort.md)

```python
class V:
    def __init__(self):
        self.a = None
        self.b = None
v = V(); v.a = 1; v.b = 2
print(v.a, v.b)
```

Here both fields DO get slots (`e12 /* a */`, `e13 /* b */`), pyc emits
**zero warnings**, and the binary aborts at run time with `matching
function not found`. CPython prints `1 2`. Filed separately as
[048](048-none-int-field-pair-runtime-abort.md) — it shares only a
discovery path with this issue.

## Verification plan

- `tests/default_arg_omitted_differently.py` must print `0`, matching
  CPython, and its `.known_issue` tag must then be deleted.
- `sat` should lose the two `'cause' illegal: str` warnings.
- Re-run the shedskin sweep: `sat` currently compiles and dies with
  "Unhandled exception"; it should run.

## What this unblocks

`sat`. More importantly it removes a **silent** wrong-answer class from
ordinary Python — defaulted keyword parameters used non-uniformly across
call sites is an extremely common shape, and today it produces no error,
just a different answer.
