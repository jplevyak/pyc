# 036 — `list.pop`/`insert`/`__hash__` and `tuple.__hash__` missing; negative-literal method defaults never worked on non-record builtins

**Status: FIXED 2026-08-06.** Found investigating
`shedskin_examples/sudoku1/sudoku1.py`.
**Affects:** `__pyc__/04_sequence.py` (`list`/`tuple` classes);
`python_ifa_build_if1.cc`'s `PY_unaryop` case in `build_if1_pyda`.
**Related:** [025](../025-shedskin-examples-coverage.md)'s "R1 missing
sequence ops" bucket (`extend`, `__rmul__`, etc. — this is the same
bucket, `pop`/`insert`/`__hash__` were the remaining gaps) and its
"Compiler bug found and fixed: default arguments on methods of
non-record builtin classes never worked" note, which explicitly
scoped its own fix to *literal* defaults and left computed defaults
(its own example: `size=-1`) unfixed for non-record builtins — this
issue closes exactly that gap by making `-1` itself no longer count
as "computed."

## Symptom

`sudoku1.py` (a recursive-backtracking-with-lookahead sudoku solver)
failed to compile:

```
sudoku1.py:104: warning: illegal call argument type 'inew' illegal
sudoku1.py:100:   inew, jnew = u.pop(c)
sudoku1.py:126:   backup = l.pop(m)
sudoku1.py:131:   l.insert(m, backup)
sudoku1.py:64:    hashcode = hashcode * 17 + hash(tuple(puzzle[c]))
```

each cascading into "expression has no type" / "unresolved call" and
ultimately a NOTYPE-poisoned `perm()` (the recursive solver core).

## Root causes (four independent gaps, one corpus example)

1. **`list.pop(index)` and `list.insert(index, value)` don't
   exist.** `__pyc__/04_sequence.py`'s `list` class had `remove`,
   `append`, `extend`, `index`, `count`, `reverse`, `sort` — never
   `pop`/`insert`. Only `set.pop()` (the unrelated zero-arg
   `set`-only form) existed anywhere in `__pyc__`.

2. **`tuple` has no `__hash__` at all**, and neither does `list` —
   which matters because pyc's own `tuple(iterable)` intercept
   (documented in `tuple.__pyc_tolist__`'s comment: dynamic-length
   `tuple()` can't be a true fixed-arity tuple struct) returns a
   **list** standing in for what the caller thinks is an immutable
   tuple. `sudoku1`'s `hash(tuple(puzzle[c]))` (a board row memoized
   as a dict key) therefore calls `.__hash__()` on a plain `list`,
   which — even after fixing #1 — still had none. `hash(x)` dispatches
   to `x.__hash__()` (`05_builtins.py`); with neither `tuple` nor
   `list` defining it, the call never resolves.

3. **Negative-literal method defaults never work on non-record
   builtin classes (list/tuple/str/...).** Once `pop`/`insert` were
   added with the idiomatic `def pop(self, index=-1)`, any call
   omitting the argument (`l.pop()`) still failed identically to #1's
   original symptom. Root cause, confirmed by isolation (a
   free function default, a user-class method default, and a
   `list`-method default with a *positive* literal all worked; only a
   `list`-method default with a *negative* literal failed): this is
   exactly the gap [025](../025-shedskin-examples-coverage.md)'s own
   "default arguments on non-record builtins" fix left open —
   `gen_fun_pyda` (`python_ifa_build_syms.cc`) only skips the
   global+MOVE initialization path (which non-record builtins'
   methods can never execute, since it normally runs in the class
   body `___init___` closure that `gen_class_pyda` only calls for
   `Type_RECORD` classes) for a default whose rval is *already* a
   plain constant. `-1` never was one: `PY_unaryop` (the AST node for
   any leading `+`/`-`/`~`) *always* lowered to a runtime
   `__neg__`/`__pos__`/`__invert__` send — even when the operand is
   itself a numeric literal — so `index=-1`'s rval was a fresh SSA
   temp fed by a `SEND`, not a constant, and 025's literal-only fast
   path never triggered. (`file.read(self, size=-1)` was unaffected
   only because `file` is an ordinary `Type_RECORD` class, not a core
   non-record builtin — the same reason `sort(self, key=None,
   reverse=False)`'s all-non-numeric defaults worked fine.)

## Fixes

1. **`list.pop`/`list.insert` added** (`__pyc__/04_sequence.py`),
   Python-semantics-matching (negative/out-of-range index clamping
   for `insert`; negative-index normalization for `pop`), built from
   existing primitives only (`append`'s in-place resize + a shift
   loop for `insert`; `self[i]` + `__delitem__` for `pop`) — no new
   compiler primitives needed.
2. **`tuple.__hash__` and `list.__hash__` added**, identical simple
   polynomial combiner (CPython's pre-SipHash tuple multiplier,
   `1000003`) over an index loop calling each element's own
   `__hash__()`. An index loop is safe here (unlike `tuple`'s
   `__eq__`/`__lt__`, which need the per-arity-unrolled generated
   form) because the only operation on `self[k]` is dispatching a
   method whose result type doesn't depend on which heterogeneous
   branch was taken — the same reasoning `__str__`/`__contains__`
   already rely on. `list.__hash__` is a deliberate, narrow CPython
   divergence (real lists aren't hashable) needed only because pyc's
   own `tuple(iterable)` compromise hands back a `list`; exact hash
   values don't need to match CPython, only be self-consistent within
   one run (matches `str.__hash__`'s own documented convention).
3. **`PY_unaryop` constant-folds a numeric literal operand**
   (`python_ifa_build_if1.cc`): when `n->op` is `PY_OP_USUB` or
   `PY_OP_UADD` and the single operand is a `PY_number` node, build
   the (possibly negated) constant `Sym` directly via `make_num_pyda`
   (negating its `Immediate` and re-interning through `if1_const` for
   `USUB`) instead of emitting a `__neg__`/`__pos__` send — no `code`,
   `rval` is a genuine constant. Falls through to the original
   runtime-dispatch path for anything else (`-x`, `--1`, `~n`), so
   only the literal case changes. This isn't just a default-arg fix:
   it removes a needless runtime dispatch for every negative/positive
   numeric literal anywhere in any pyc program (`x = -1`, `f(-2.5)`,
   array bounds, etc.), and it is what makes `index=-1`'s rval a
   plain constant, closing gap #3 through 025's *existing* fast path
   rather than adding a new one.

## Verification

- New tests, both backends, output byte-identical to `python3`:
  `tests/list_pop_insert.py` (pop with/without index, negative index,
  insert at start/middle/end/past-end/negative/empty-list), `tests/
  tuple_hash.py`, `tests/list_hash.py` (self-consistency + the
  `tuple(a_list)` round-trip `sudoku1` actually needs).
- `test_pyc.py`, C and LLVM backends, `PYC_CSM` unset: 247/11/0/4 both
  (244 baseline + 3 new tests, 0 regressions).
- `test_pyc.py`, C and LLVM backends, `PYC_CSM=2`: 243/11/4/4 both,
  same 4 pre-existing failures as baseline.
- `shedskin_sweep.sh`: clean before/after comparison from the same
  commit (stashed this change, swept, popped, rebuilt, swept again —
  not a diff against a stale multi-day-old snapshot). Net: **zero
  regressions**; three unrelated corpus examples improved as a side
  effect of fix #3 alone (`brainfuck`, `kanoodle`: warned-compile →
  clean compile; `rubik`: FAIL → compiles-with-warning). `dijkstra`/
  `loop`/`sat`/`solitaire`/`sudoku5` show only a shifted diagnostic
  line for the same pre-existing, unrelated failure — not a status
  change.
- `sudoku1.py`: now compiles (one remaining harmless warning, see
  below) and **runs to completion, output byte-identical to
  `python3`** (including the exact `iterations: 35606` solver-step
  count), across the full `for x in range(10): for y in range(20):
  main()` timing loop — and faster than CPython (4.00s vs 5.05s in
  one comparison run).

## What's still open (not part of this issue, mapped to an existing one)

`sudoku1.py` still emits one compile warning
(`sudoku1.py:82/88: expression has no type`, on `(u == []) and (l ==
[])` / `l == []`) from inside `list.__eq__`'s index loop
(`l[i] != self[i]`) being typed against an empty-list-literal
argument's bottom/no element type — even though that loop is
provably dead whenever the comparison actually reaches it (arities
differ, or `range(0)` is empty). This is **not** a new bug: it's the
well-tracked, already-designed-and-negative-prototyped
[ifa/072](../../ifa/issues/072-empty-container-notype-current-mechanism-and-plan.md)/[ifa/043](../../ifa/issues/closed/043-empty-container-inference-options.md)
"empty container element inference" family (a *read that returns the
element as a value* on a never-written container has no type; 072's
own prototyped fix for this exact shape measured net-negative on the
corpus and was withdrawn). Confirmed it doesn't block `sudoku1` at
runtime — the warning sits on a dynamically-dead branch for every
puzzle this program's `main()` actually solves — so it's left as-is
rather than attempted here.

## What this unblocks

- `shedskin_examples/sudoku1/sudoku1.py` (a corpus benchmark): FAIL →
  compiles and runs correctly end-to-end.
- Any program using `list.pop(i)`/`list.insert(i, x)` (previously
  silently "unknown method" territory, degrading to NOTYPE the same
  way `extend` did before 025's fix) or hashing a tuple/list
  (`dict`/`set` keyed by tuples, `hash(tuple(...))`-style
  memoization).
- Any negative or explicitly-positive numeric literal anywhere in any
  pyc program now constant-folds instead of dispatching a runtime
  `__neg__`/`__pos__` send — a small general codegen/perf win
  independent of the default-arg motivation.
- Any other non-record-builtin method with a negative-literal default
  argument (the general form of gap #3), not just this issue's
  `list.pop`.
