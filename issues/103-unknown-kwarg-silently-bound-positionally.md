# 103 — an unrecognized keyword argument is silently bound to the next positional parameter

**Status:** open, root-caused 2026-08-16 while digging into
[ifa/issues/102](../ifa/issues/102-corpus-programs-compile-then-abort-at-runtime.md)
class A. Repro: `tests/unknown_kwarg_rejected.py` (`.known_issue`).
**Silent wrong behaviour, and the cause of at least one corpus program's
runtime abort.**

## Symptom

```python
def f(A, B=None, C=None):
    print(A)
    print(B is None)

f([1, 2], nosuchkw=99)
```

| | result |
|---|---|
| CPython | `TypeError: f() got an unexpected keyword argument 'nosuchkw'` |
| **pyc** | prints `[1, 2]` then **`False`** — `nosuchkw`'s value was bound to `B` |

No diagnostic at any stage. The program compiles and runs.

> **Binding site FOUND and the misbinding fixed 2026-08-16**
> (`PYC_KWSTRICT`, on by default). See "Cause: located" below. The fix is
> partial: the silent misbinding is gone and there is now a call-site
> diagnostic, but it is a *warning* with generic wording, and the
> unmatched call is elided rather than rejected — so the program runs and
> prints nothing instead of raising `TypeError`. The issue stays open for
> the message and for
> [102](../ifa/issues/102-corpus-programs-compile-then-abort-at-runtime.md)'s
> "codegen should not silently emit an abort stub".

## Cause: located

Working backwards from the emitted C rather than forwards from the
matcher found it. `print(B is None)` had been **constant-folded to the
literal `"False"`**, so `B` was bound at the type level and the parameter
then optimised away. Instrumenting `Matcher::build` at the
`default_wrapper` call gives the decisive line:

```
[kwmap] fun=f defaults=1 | nactuals=3
```

**Three actuals, one defaulted formal, and an empty actual→formal map.**
The named actual never leaves the positional sequence: it keeps slot 3,
the identity position map sends slot 3 to formal 3 (`B`), `C` is
defaulted, and `default_wrapper` assigns 99 to `B`.

So nothing "fails to reject" it in the *named* path — that path empties
the candidate list correctly. **The surviving match comes from the
positional route, where the name is never consulted.**

The check therefore belongs at the top-level entry point
(`ifa/if1/pattern.cc:1633`, after `find_all_matches`), which is where both
the candidate list and `names` are in scope. A candidate with no formal
of a given actual's name is dropped.

Corpus, 77 programs: **zero exit-code changes**, and only `life` changes
at all (violations 30 → 35, `ess` 135 → 117, `css` 708 → 688) — so the
generated code for the other 76 is identical. Suite unchanged.

### Scope correction

Of six class-A crashers checked, **only `life` has any rejections** (15);
`othello`, `mwmatching`, `rubik`, `amaze` and `kmeanspp` have zero. This
explains `life` specifically, **not**
[102](../ifa/issues/102-corpus-programs-compile-then-abort-at-runtime.md)
class A generally, which the first version of this issue implied.

`life` also still aborts under the fix (abort stubs 15 → 16): rejecting
the match means "no matching function", which `cg.cc:2055` still turns
into a runtime stub. Making `life` work needs `product(repeat=)`.

## Superseded: earlier hypotheses (both falsified)

The behaviour is certain and reproducible; **the internal mechanism is
not**. Two hypotheses were formed and both are falsified by measurement,
recorded here so they are not re-tried:

1. **"`build_positional_map` binds it positionally."** The idea was that
   `f->named_to_positional.get(fcnp)` returns null for an unknown name
   and the actual then falls into the *"collect actual positions not used
   by named arguments"* sweep. A probe on that exact null case **never
   fires** — not on the reproducer, and not on any of ten class-A corpus
   crashers.
2. **"`find_all_matches` fails to disqualify the candidate."** A
   rejection was implemented there behind a flag. It never fires either:
   instrumenting shows that at the point the named actual is processed,
   `some_named=1` but the candidate list is already **`n=0`** — so
   `find_arg_matches` *had* correctly eliminated the candidate on the
   named position. The named path is working.

So the misbinding happens somewhere else: on a path that runs after all
candidates are eliminated, or on a call route that does not go through
`find_all_matches` with names at all. The frontend does attach the name
(`if1_add_send_arg(..., cannonicalize_string(kw_keys[ki]->str_val))` in
`python_ifa_build_if1.cc`), and the matcher does see it — `PYC_DBG_BADKW`
prints `[kwseen] named actual 'nosuchkw' at pos 3` — so the name is not
lost in lowering.

**Next probe should start from the other end**: find what *does* bind the
value to `B`, rather than what fails to reject it. A default-argument
wrapper (`default_wrapper` / `order_wrapper` in `python_ifa_sym.cc`) is
the obvious suspect, since those synthesize a forwarding call with
positional formals.

## How this reaches a runtime crash

`shedskin_examples/life` is the worked example, and it is
[102](../ifa/issues/102-corpus-programs-compile-then-abort-at-runtime.md)'s
cheapest class-A reproducer:

1. `life.py` calls `product((0, 1), repeat=rows*columns)`.
2. `pyc_lib/itertools.py` defines `product(A, B=None, C=None, D=None)` —
   **there is no `repeat` parameter**.
3. `repeat`'s value (an `int`) is silently bound to `B`.
4. `B is not None` now, so the body takes the `elif C is None:` branch and
   runs `for b in B:` — **iterating an integer**.
   Confirmed directly — compiling `product((0,1), repeat=3)` makes pyc
   point at the exact line:

   ```
                   for b in B:
                              ^
     called from itertools.py:14
   ```
5. `int` has no `__iter__`, so the send has *no* candidate functions.
   `PYC_DBG_DISPATCH` shows it exactly:
   `DISPATCH FAIL in product: fns=-1 rvals=2 | r0=__iter__:symbol r1=_:int64`
6. FA types the result `void_type` (bottom); every downstream use is
   unresolvable, and `cg.cc:2055` emits
   `assert(!"runtime error: matching function not found")` rather than
   failing the build.
7. `life` compiles cleanly and aborts when the loop is entered.

All nine of `life`'s dispatch failures are class A, and this is the
upstream cause of them.

## Fix direction

**Locate the binding site first** — see above; two plausible-looking
sites have already been ruled out by measurement, so the next attempt
should be evidence-led rather than another guess. This area is delicate:
the comment on `PycCompiler::order_wrapper` (`python_ifa_sym.cc`) records
how [087](../ifa/issues/closed/087-DISPATCH-out-of-order-keyword-args.md)'s
out-of-order keyword matching failed *far* downstream when a callback
silently returned 0 — the same class of silent-fallthrough failure.

Then **report it properly**: CPython's wording (`f() got an unexpected
keyword argument 'nosuchkw'`) at the call site, which is what
`tests/unknown_kwarg_rejected.py.check` pins.

### `product(repeat=)` — implemented 2026-08-16

`pyc_lib/itertools.py`'s `product` now accepts `repeat`. Verified against
CPython: `product((0,1), repeat=3)` gives all 8 rows in CPython's exact
order (`000, 001, 010, 011, 100, …`).

**Its elements are LISTS, not tuples, and that is deliberate.** `repeat`
is a runtime value while pyc's tuples are fixed-arity records, so no
tuple type can be given. shedskin sidesteps this by having a
variable-length homogeneous `tuple<T>` — its `itertools.py` is only a
type stub (`yield iter(iterables).__next__(),`, yielding a 1-tuple
whatever `repeat` is) with the real work in C++. pyc has no such type.
Lists support iteration, indexing, `len` and `zip`, which is what
`repeat` is used for in practice.

`sudoku5`, the corpus's only other `product` user, uses the two-argument
form and is byte-for-byte unaffected.

### `life` is now blocked on something else

Abort stubs drop 15 → 12, but it still aborts, at a *different* site. The
remaining dispatch failures are:

```
DISPATCH FAIL in __init__: fns=-1 rvals=2 | r0=__iter__:symbol r1=initial:?
DISPATCH FAIL in process:  fns=-1 rvals=2 | r0=_:void_type r1=board:void_type
DISPATCH FAIL in snext:    fns=-1 rvals=2 | r0=__iter__:symbol r1=_:void_type
```

`pyc_lib/collections.py`'s `defaultdict.__init__(self, factory=None,
initial=None)` does

```python
if initial:
    for k in initial:
```

and `life` calls both `defaultdict(int)` and `defaultdict(int, board)`,
so `initial` is the union `{None, defaultdict}` at the iteration site.
`None` has no `__iter__`, so there is no single candidate — and the `if
initial:` guard does not narrow the type. That is the
[018](018-dict-mixed-key-types-boxing-failure.md) family, **not** this
issue, and the `void_type` operands in `process`/`snext` are downstream
consequences of it.

So `life`'s class-A failures had *two* independent causes, and this issue
accounts for one of them.

## Verification plan

- `tests/unknown_kwarg_rejected.py` reports an error instead of printing
  `False`; delete its `.known_issue` tag.
- Re-run `ifa/issues/runstatus.sh`: `life`'s nine class-A dispatch
  failures should disappear (it will then fail to *compile* until
  `product(repeat=)` exists, which is the honest state).
- Check the corpus for other unknown-keyword calls before landing — this
  changes calls that currently "work" by accident, and some corpus
  programs may depend on the misbinding.

## What this unblocks

Part of [102](../ifa/issues/102-corpus-programs-compile-then-abort-at-runtime.md).
More importantly it closes a silent-wrong-answer hole: today any typo in
a keyword argument, or any call against a `pyc_lib` stub whose signature
has drifted from CPython's, binds a value into the wrong parameter with
no diagnostic at all.
