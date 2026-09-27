# 122 — `list` has no ordering comparisons (`__lt__`/`__le__`/`__gt__`/`__ge__`)

**Status:** CLOSED 2026-09-27 -- list has lexicographic ordering; see "Fixed" at the end.
**Affects:** `__pyc__/04_sequence.py` — `class list` (~line 41) defines
`__eq__`/`__ne__` but none of the four ordering dunders. `str`, `bytes`,
`int`/`float` and `tuple` all have them (`01_str.py`, `01b_bytes.py`,
`02_numeric.py`, `04_sequence.py`'s tuple unroll); `list` is the gap.
**Found:** surfaced by [ifa/issues/098](../../ifa/issues/closed/098-FA-per-pass-reset-scoped-to-reachable-set.md)'s
second-defect fix, which stopped a total dispatch failure from being
silently swallowed by `collect_argument_type_violations`. The missing
methods had been there all along; nothing reported them.

## Symptom

```python
print([1, 2] < [1, 3])     # CPython: True
print(max([(1.0, [2]), (1.0, [3])]))   # CPython: (1.0, [3])
```

pyc emits `warning: unresolved call '__lt__'` (permissive mode; a hard
error under `--strict`) and the comparison's result is left untyped, so
every consumer downstream of it degrades — on `mastermind2` this is what
leaves six `if`s with bottom-typed conditions, which in turn stops
`add_pnode_constraints`' `Code_IF` case from walking either branch.

The corpus hit is not a contrived one. `mastermind2.py:19` and `:127`
are

```python
_, play = max([(utility(play, possibles), play) for play in plays])
```

— a `(float, list)` tuple compare, which falls through to comparing the
`list` halves whenever the utilities tie. That is idiomatic
"max by key with a tiebreak" Python and it is unresolvable today.

## Why it was invisible until now

`EntrySet::out_edge_map` is never reset (it cannot be — `get_AEdges`
reads it for cross-pass edge identity), and
`collect_argument_type_violations` treated "the map has an entry for
this send" as "this send dispatched". An entry survives from the first
pass in which dispatch succeeded, so a pass in which dispatch fails
completely still took the `else` arm, found no analyzed edge to
inspect, and reported nothing. Fixed 2026-08-31 by testing
`EntrySet::out_edges` (which IS reset per pass) instead; see
[ifa/issues/098](../../ifa/issues/closed/098-FA-per-pass-reset-scoped-to-reachable-set.md),
"The second defect's fix".

## Proposed fix

Add the four methods to `class list` in `__pyc__/04_sequence.py`,
lexicographic per CPython: compare element-wise to the shorter length,
first difference decides, otherwise the shorter list is less. `__eq__`
is already there and already element-wise, so `__lt__` is the only real
body; `__le__`/`__gt__`/`__ge__` delegate the way `tuple`'s do
(`04_sequence.py:478-483`).

Two things to check while writing it, both already load-bearing
elsewhere in this file:

- **Element type.** `[1,2] < [1,3]` needs `__lt__` on the *elements*,
  so the method is only as resolvable as the element type is. A list of
  a type with no ordering should produce a diagnostic naming the
  element, not the list.
- **The reflected fallback.** `object.__gt__`/`__ge__`
  (`__pyc__/00_runtime.py:112-121`) exist so a class defining only
  `__lt__` gets `>` for free; defining all four on `list` directly is
  simpler and matches `tuple`, but confirm it does not shadow that path
  for list subclasses.

## Verification plan

1. `tests/list_ordering.py` — the repro above plus the tie-break shape,
   with `.exec.check` holding CPython's output, and a
   `.known_issue` sidecar naming this issue until the fix lands (per
   `issues/README.md`). It flips to `PASS` by itself.
2. `make test` — the five CI steps in CLAUDE.md's order.
3. `./corpus_sweep.sh -m check` against the pre-fix tree. Expect
   `mastermind2`'s warning count to drop and its six bottom-typed `if`s
   to resolve; `check` mode, not `compile`, because the interesting
   outcome is whether the newly-typed branches change what the binary
   prints.

## What this unblocks

`mastermind2`'s six unreachable-by-analysis branches, and any corpus
program that sorts or max/mins tuples whose tail is a list. It is also
the honest half of ifa/098's remaining symptom: 098 made the failure
visible, this makes it not fail.

## Fixed (2026-09-27)

`list.__lt__`/`__gt__`/`__le__`/`__ge__` (`__pyc__/04_sequence.py`),
lexicographic as in CPython: the first unequal element decides, otherwise
the shorter list is smaller. Each is written out rather than sharing a
helper that takes the other list (RUNTIME.md). `tests/list_ordering.py`
flips to PASS on both backends and its `.known_issue` is removed.
`tests/list_ordering_lexicographic.py` adds empty lists, ties and `sorted`.

It was the first of five defects between `shedskin_examples/mastermind2`
and CPython's output, and it accounted for all 44 of its compile errors'
root. The other four, fixed with it:
- `math.log` had no base argument (`log(p, 2)`).
- LLVM passed an `int` argument's bits to a C function declared to take a
  double: `math.sqrt(4)` returned `2.2e-162`, silently. Numeric arguments
  to `__pyc_c_call__` are now converted to their declared type.
- `%r` in `%`-formatting reached `vsnprintf`, which printed it literally
  and consumed no argument, shifting the rest; `str.__repr__` did no
  escaping or quote selection.
- `random.sample` did not follow CPython's algorithm, so a seeded run drew
  a different sequence.

mastermind2 now matches CPython on both backends (C 32 s, LLVM 6 s,
CPython 33 s).
