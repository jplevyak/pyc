# 116 — a class with `__iter__`/`__next__` iterates zero times, silently

**Status:** CLOSED — fixed 2026-08-22 (`61d71524`). Found 2026-08-21 while clearing
issues/115's residue. See the FIXED section at the bottom -- the
plan sketched under "Fix" below turned out **not to be
implementable as written**, and what shipped is a different shape.

## Symptom

```python
class Counter:
    def __init__(self): self.n = 0
    def __iter__(self): return self
    def __next__(self):
        self.n += 1
        return self.n

for x in Counter():
    if x > 3: break
    print(x)
```

    CPython:  1 2 3          pyc:  (no output, exit 0)

No warning, no error, exit 0. The class implements the standard Python
iterator protocol exactly and the loop body simply never runs.

## Cause

pyc's for-loop is **peek-then-fetch**, not fetch-until-StopIteration.
`PY_for_stmt` (python_ifa_build_if1.cc) lowers to

    iter = obj.__iter__()
    while iter.__pyc_more__():
        x = iter.__next__()

and `object.__pyc_more__` (`__pyc__/00_runtime.py`) returns **False**:

```python
class object:
    def __null__(self):
        return False
    def __pyc_more__(self):
        return False
```

So any class that doesn't override `__pyc_more__` is reported empty.
Every iterator in `__pyc__/` defines one, which is why nothing in the
suite caught it — but `__pyc_more__` is not a Python method, so no user
code and no ported library will ever define it.

Two protocols with different shapes:

| | pyc | CPython |
|---|---|---|
| more? | `__pyc_more__()` → bool | (no equivalent) |
| next | `__next__()` | `__next__()`, raises StopIteration |

Peek-then-fetch is the cheaper shape for a compiler — no exception on
the hot path — and `__pyc_generator__` shows the bridge is buildable:
it advances eagerly and remembers the value in `nextval`/`primed`.

## Fix

Bridge the two in the for-loop lowering, not on `object`: when the
receiver defines `__next__` but no `__pyc_more__`, lower the loop to
CPython's shape instead -- call `__next__` inside a try, catch
StopIteration, break. issues/011's exception machinery already provides
everything needed, and generators/builtins keep the cheap path because
they do define `__pyc_more__`.

**This does not work.** "When the receiver defines `__next__`" is not a
question build_if1 can answer: the loop is lowered from the AST, long
before flow analysis, and `for x in f()` gives it no receiver type at
all. The decision has to be made where the *class* is, not where the
loop is -- see FIXED below.

A default on `object` cannot work: bridging needs somewhere to stash the
peeked value, and a base-class method has no per-instance state to use
without adding fields to every object in the program.

Whatever the shape, **`object.__pyc_more__` returning False must go**.
Silently reporting a non-empty iterator as empty is the worst available
answer; if the protocol can't be satisfied, the call should not resolve.

## What this blocks

`pyc_lib/itertools.py`'s `count` was the live instance — `for j in
count(...)` yielded nothing, so `shedskin_examples/sunfish`'s
`gen_moves` scanned every ray as empty. Fixed directly there (a `count`
is infinite, so its `__pyc_more__` is `return True`), covered by
`tests/itertools_count_forloop.py`. It was the only such class in
`pyc_lib/` or `__pyc__/`, but it is exactly the shape any ported
library or user class will have.

## Verification plan

- The repro above prints `1` / `2` / `3`.
- A finite `__iter__`/`__next__` class raising StopIteration terminates
  the loop at the right point.
- `list(obj)`, `in`, and comprehensions over such a class agree with
  CPython.
- Generators and builtin containers keep the `__pyc_more__` path (no
  new try/except in their emitted code).
- `tests/itertools_count_forloop.py` still passes.

---

## FIXED (2026-08-22)

A `__pyc_iterator__` bridge **class**, not a per-loop lowering choice.

### The shape that works

The decision "does this thing speak CPython's protocol?" is answerable
at the *class definition*, where the method names are literally in the
AST — not at the loop, which has no receiver type before FA. So:

1. **build_syms** (`python_ifa_build_syms.cc`), on `PY_classdef`, sets
   `PycScope::iter_bridge` when a **non-builtin** class body defines
   `__next__` and not `__pyc_more__`. It is set *before* the body walk,
   because step 2 happens during it. The bases are already resolved by
   then (PY_classdef walks the header before the body), which is what
   makes the two ancestor queries under "Inheritance" below possible at
   that point — `sym->implements` is not filled until afterwards.
2. While walking that body, `PY_funcdef` installs the class's own
   `__next__` under the member name **`__pyc_user_next__`** instead
   (a rename of `def_name` only — the function itself is untouched).
3. After the bases are resolved, `__pyc_iterator__` is added as a base
   with `inherits_add`.
4. `__pyc_iterator__` (`__pyc__/00_runtime.py`) supplies the
   peek-then-fetch pair in front of it — `__pyc_more__` fetches one
   value ahead inside a `try`/`except StopIteration` and remembers
   whether there was one, `__next__` hands back what was peeked — plus
   `__contains__` (`x in it`) and `__pyc_tolist__` (`list(it)`), which
   pyc lowers to direct dispatches with no iterable fallback.

`__pyc_peek__` gets **no class-level initializer**: an `= 0` would pin
the value channel to `int` for every bridged class, which is exactly
the bug issues/114 fixed in `__pyc_generator__`. Its type comes from
`__pyc_user_next__`'s return type.

`object.__pyc_more__` is **gone**, as this issue demanded. With the
bridge in place the only shape it could still catch is a class whose
`__iter__` returns something implementing neither protocol, and an
unresolved call is the right answer there. Nothing in `tests/`,
`pyc_lib/`, or `shedskin_examples/` relied on it — every `__iter__` in
the corpus and library delegates to `iter(list)`, which returns a real
builtin iterator.

### Inheritance: rename and add-base are separate decisions

"Body defines `__next__` and not `__pyc_more__`" is the right question
only for a class with no relevant base. Three shapes, decided from the
resolved base Syms:

| a base… | rename `__next__` | add the bridge |
|---|---|---|
| — (none relevant) | yes | yes |
| already derives from the bridge | yes | **no** |
| defines its own `__pyc_more__` | **no** | **no** |

The middle row is a subclass **overriding** `__next__`. It must be
renamed, so the override lands on the name the `__pyc_more__` it already
inherits actually calls — but adding the bridge again would list it
alongside a base that derives from it, which C3 rejects ("inconsistent
precedence graph", the same failure as row 1 of bug 1 below).

The bottom row is a base that speaks pyc's protocol natively. Its
`__pyc_more__` pairs with this class's `__next__` under the real name;
bridging would both break that pairing and give `__pyc_more__` two
unrelated candidates, which dispatch reports as ambiguous.

`pyc_class_or_base_is` / `pyc_class_or_base_defines` (build_syms) answer
the two queries by walking `implements` transitively.

### Three bugs stood behind it

**1. C3 linearization rejected the bridged class, twice.** First
because `__pyc_iterator__` was defined before `class object` in
`00_runtime.py` (it must come after — it subclasses it). Then because a
bridged class got bases `[object, __pyc_iterator__]` while the bridge
itself subclasses `object`: an inconsistent precedence graph. Fixed by
suppressing the implicit `object` base when `iter_bridge` is set — the
bridge brings it.

Sibling bases were the first attempt and are genuinely wrong here, not
just unlucky: `class __pyc_iterator__:` alongside `object` made
`__pyc_more__` an **ambiguous call** with two candidates. pyc resolves
methods by pattern specificity on `self`, not last-store-wins on the
prototype slot, so two unrelated bases defining one name is ambiguous
by construction.

**2. `except StopIteration` in the builtin module was dead code.** The
bridge's whole mechanism is a `try` inside `__pyc__/00_runtime.py`, and
`emit_exc_check` returned early for *all* builtin code. Narrowing that
exemption to "no check unless the `try` is in this same builtin
function" was necessary but not sufficient: the builtin module's IF1 is
built in the **baseline pass**, before `pyc_program_has_raise` is set
(`python_ifa_main.cc`'s loop starts at module 1), so the flag every
other caller is gated on is *always false there*. The builtin-own-try
case is therefore deliberately ungated. Symptom before the fix: a
bridged loop printed `1 2 3 1 1 1 …` forever — the exception ran
straight past the handler to "Unhandled exception".

**3. A for loop never checked for a pending exception after advancing
its iterator** — `python_ifa_build_if1.cc`'s `PY_for_stmt` and
`build_list_comp_pyda` called `__pyc_more__`/`__next__` with no
`emit_exc_check`, the only call sites in the compiler that skipped it.
This is **pre-existing and independent of this issue**: a generator body
that raised reported "Unhandled exception" instead of reaching the
enclosing `try`, which had nothing to do with the bridge. It surfaced
here because a bridged class whose `__next__` raises something *other*
than StopIteration propagates out of `__pyc_more__`, and with the check
missing the loop re-served its last peeked value forever.

### Verification

`tests/iterator_protocol_bridge.py` (+ `.exec.check`) covers the whole
plan above and then some: the for loop, a comprehension, `list()`,
`in` (True and False), a subclass inheriting the bridge, a subclass
**overriding** `__next__`, a subclass of a class that speaks pyc's
protocol natively, two instances keeping independent state, nested loops
over separate iterators, a `Fast` class defining `__pyc_more__` keeping
the non-bridged path, a non-StopIteration exception propagating out of
the loop, and the same for a raise from a generator body.

That `Fast` really keeps the cheap path is checked by reading the
emitted C: `__pyc_user_next__` appears for `Counter`/`Sub`/`Bad` and
never for `Fast`, which still emits its own `__next__` and
`__pyc_more__`.

`tests/itertools_count_forloop.py` still passes; `pyc_lib/itertools.py`'s
`count` keeps its hand-written `__pyc_more__` (it is infinite, so the
bridge would be pure overhead).

Both suites: **293 passed, 0 failed** on the C backend and on
`PYC_FLAGS=-b`.

`tests/minmax_3arg.py.check` needed its embedded `__pyc__.py` line
numbers moved (issues/111) — `00_runtime.py` grew by the bridge class.

### One bug found and NOT fixed here

Verifying the inheritance cases turned up **ifa/issues/110**: a subclass
that overrides a method gets a *second* member slot for it rather than
replacing the inherited one, shifting every field after it, while a
sibling method shared with the base still blind-casts to the base's
layout. Nineteen lines with no iterator protocol in them hang forever,
and it reproduces unchanged on `888cb499`.

It is filed, not fixed — the dedup lands in `ifa/if1/ast.cc`'s
`collect_include_vars` and changes every record layout in every program,
which needs its own verification pass. It touches this issue in two
places: bridged subclasses carry the `__pyc_iterator__` member set twice
(harmless today only because `__pyc_more__` is cloned per receiver
there), and the test's "base speaks pyc's protocol natively" case uses a
`FastBase` that deliberately does not define `__next__`, so that its
subclass adds rather than overrides.
