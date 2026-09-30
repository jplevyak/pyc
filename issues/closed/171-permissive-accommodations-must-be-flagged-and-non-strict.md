# 171 — every Python accommodation must be flagged, and off under `--strict`

**Status:** closed 2026-09-29. Every accommodation in the audit is now
gated, fixed, or a documented representation. The residuals went to
[172](../172-generator-send-channel-and-stopiteration-value.md),
[173](../173-silent-deviations-found-by-the-strict-suite-check.md),
[007](../007-decorators-not-applied.md), [128](../128-cross-class-field-promotion.md)
and ifa/156 ("What remains"). The RULE stays in force: every new
accommodation must meet it. Filed 2026-09-28 from the author's directive:
*"all python specific features are under flags … all such accommodations
should be in non-strict mode."* This is the audit, with a verdict per
item. The policy it enforces is in AGENTS.md ("Generalised … every
Python-specific accommodation is flagged and non-strict") and, for corpus
programs, in [shedskin_examples/PYC_CHANGES.md](../../shedskin_examples/PYC_CHANGES.md.

## The rule

An **accommodation** is any behaviour beyond CPython 3 semantics or
beyond static typing:

- accepting something CPython rejects;
- changing a value's type or representation so the program can be
  typed;
- returning a different kind of object than CPython does;
- silently reading a value CPython would raise on.

Every accommodation must:

1. **Be behind a named flag.** It is on under `--permissive` (the
   default) and **off under `--strict`**, where the program is refused
   with a diagnostic naming the type error.
2. **Warn when it fires**, if it changes observable behaviour, naming
   the deviation.
3. **Reach generic ifa only through `IFACallbacks`.** ifa is not a
   Python compiler.

When a corpus program needs an accommodation that is unreasonable, or
that would change what a correct program prints, edit the program
instead (PYC_CHANGES.md).

`--strict` then means exactly one thing: **CPython semantics, or a
compile error.**

## Audit (2026-09-28, at `e3b44e2c`)

| # | accommodation | where | flag / gating today | verdict |
| --- | --- | --- | --- | --- |
| 1 | `raise "message"` wrapped in `Exception(...)` (a CPython 3 `TypeError`) | `python_ifa_build_if1.cc` (`PY_raise_stmt`) | **DONE 2026-09-28:** permissive only (`PYC_RAISE_STRING`, default on in permissive); warns at each site; `--strict` refuses (`tests/raise_string_strict.py`) | ✓. Corpus sites `chess.py:92,128` and `minilight/ml/entry.py:77` now warn by default and are refused under strict. They remain edit candidates. |
| 2 | implicit-`None` fall-off replaced by a "typed default" | `python_ifa_build_syms.cc` (`ifa_no_implicit_none`) | **DONE 2026-09-28:** no longer set by `--strict`; `PYC_NO_IMPLICIT_NONE=1` honoured only in permissive, with a warning (ignored, with a warning, under strict). `tests/implicit_none_strict_refuses.py` | ✓. Finding: it was not even a typed default. Dropping the fall-off arm let FA fold the return to the one explicit value, so `f([0])` printed `1` where CPython prints `None`. `--strict` did that silently. |
| 3 | `{int, float}` widened to `float` (prints `1.0` for `1`) | `fa_coerce_numeric_confluences` / `coerce_annotate` | `fruntime_errors` (ifa/145). **Warning DONE 2026-09-29:** `show_numeric_coercions` (`fa.cc`) reports every coerced variable once per line after convergence, naming it and the widened type | ✓ Gated and announced. Three suite `.check` files carry it (`numeric_unification`, `mixed_numeric_field`, `builtins_batch`). Still open: prefer a split where one exists ([ifa/156](../../ifa/issues/156-FA-split-int-from-float-coerce-last.md). |
| 4 | `tuple(iterable)` returns a **list** | `python_ifa_build_if1.cc:958` | **DONE 2026-09-28:** `PYC_MAKESEQ=1` is the default, so `tuple(x)` is a real runtime-length tuple in every mode (`PYC_MAKESEQ=0` restores the list) | ✓ Nothing left to gate. The blocker was tuple deepcopy ([110](../110-tuple-from-iterable-returns-a-list.md). |
| 5 | `zip`/`map`/`filter`/`reversed` return eager lists; `itertools.product(repeat=)` yields lists | `__pyc__/05_builtins.py:222`, `pyc_lib/itertools.py` | **`product` DONE 2026-09-29:** `repeat=N` yields `tuple(row)` (runtime-length tuples, `tests/itertools_product_repeat_tuple.py`) | ✓ for `product`. **Author's decision (2026-09-29): the eager lists stay**, as a documented representation. They are equivalent under iteration and `list()`. They are observable only through `len(zip(..))`, `zip(..)[i]` or printing the object, which CPython rejects or prints as `<zip object>` (`tests/zip_builtin.py`; see [173](../173-silent-deviations-found-by-the-strict-suite-check.md). |
| 6 | `None` read as the zero value (`None + "y"` gives `"y"`) | nullable-pointer representation (ifa/164) | **DONE 2026-09-28** in every mode: a `{None, T*}` receiver is checked where the call is emitted and reports CPython's uncaught `TypeError` (`_CG_none_receiver`), and `{None, scalar}` is refused at FA time (a BOXING violation) | ✓ `tests/none_receiver_raises.py`. Remaining: the check reports and exits, so `except TypeError` cannot catch it ([ifa/165](../../ifa/issues/165-none-reaching-an-operation-is-silently-accepted.md). |
| 7 | a generator's return / `send` value smuggled through `int64` (`0` where CPython gives `None`) | `__pyc__/09_generator.py`, `P_prim_yield` | **RETURN value DONE 2026-09-28**, every mode. **`send` channel GATED 2026-09-29:** a yield whose value is used warns in permissive (`PYC_YIELD_INT_SEND`) and is refused under `--strict` | ✓ Flagged and non-strict. The typed send channel, and `StopIteration.value` through the global exception slot, moved to [172](../172-generator-send-channel-and-stopiteration-value.md. |
| 8 | "a write to a missing field discovers the field" | `P_prim_setter` (`ifa/analysis/fa_prims.cc`) | **DONE 2026-09-29:** `IFACallbacks::discovers_fields_by_write()`, default `false` (a MEMBER violation); `PycCallbacks` returns `true` | ✓ No behaviour change for pyc. The V-language tests pass with the default. The derived-state half is still [128](../128-cross-class-field-promotion.md. |
| 9 | locals possibly read before assignment are zero-filled | `--safe` (ifa/039) | `--safe` only ✓ | ✓ already a flag, and non-strict by construction. |
| 10 | a type violation compiled with a runtime check instead of refused | `fruntime_errors`, `convert_NOTYPE_to_void` | ✓ gated. `PYC_STRICTVIOL=1` (closed ifa/158) currently makes every violation fatal in every mode anyway | ✓ consistent. Note that permissive currently refuses too. |
| 11 | `range` is its own iterator (`__iter__` returns `self`), so a range iterates once | `__pyc__/05_builtins.py` | **DONE** (already in the tree at `16a380f5`): `range.__iter__` returns a fresh `__range_iter__` | ✓ `tests/range_reiterable.py`. |
| 12 | a recursive call inside a decorated function calls the UNDECORATED function | `def_internal_fn` (`python_ifa_build_syms.cc`) | **FIXED 2026-09-29**, every mode | ✓ A decorated def is no longer linked in `def_internal_fn`, so the name reads the decorated binding (`tests/decorator_recursion.py`). Finding: a NESTED decorated def skipped closure conversion, and FA aborted when it captured a local (`tests/decorator_nested_capture.py`). Its own name is now a carrier field written after decoration, and a later rebinding is refused. |
| 13 | an unhandled decorator (`@property`, dotted names) is ignored | `PY_decorated`, `inject_property_accessors` | **FIXED 2026-09-29**, every mode | ✓ `@property` getters work, dispatched per receiver class (`tests/property_getter.py`). Dotted decorators are applied (`tests/decorator_dotted.py`). Setters, and any decorator that cannot be resolved, are refused (`tests/property_setter_refused.py`). Residuals: [007](../007-decorators-not-applied.md. |

Accommodations that are **representations, not deviations**, and so are
not listed: `{None, T*}` as a nullable pointer (CPython-faithful while the
`None` is never read), and arity and prefix layouts.

## Findings while completing it (2026-09-29)

- **`--strict` refused EVERY generator** ("unable to resolve to a single
  function at call site"). This was not a generator bug: the C backend's
  `get_target_fun` failed under strict BEFORE trying polymorphic dispatch,
  so any legitimately polymorphic call was refused. The generator's is
  `e.__str__()` in `__pyc_unhandled_exception__`, over `{BaseException…,
  Exception…}`. The refusal now sits where dispatch has genuinely failed
  (`emit_send_call`, `cg.cc`), and it names the source line. Side effect:
  `loop` (`tests/list_index_type_mismatch_salvage.py`) was refused only by
  that premature check. It now compiles under strict and overflows the C
  stack, exactly as it always did in permissive
  ([173](../173-silent-deviations-found-by-the-strict-suite-check.md).
- **`sys.exit()` / `exit()` were a hard C `exit`.** CPython raises
  `SystemExit`. kanoodle catches it to end each search, so it silently
  stopped after one search with rc 0, in every mode. Both now
  `raise SystemExit(code)`, and `__pyc_unhandled_exception__` exits with the
  code, printing a `str` code to stderr with status 1, as CPython does
  (`tests/sys_exit_raises.py`). kanoodle now matches CPython.
- **`life`** went from "compiles, then aborts" to "refused": the
  `product` tuple change (#5) exposes an untyped expression in its
  `process`. See [025](../025-shedskin-examples-coverage.md.
- The strict-suite comparison found silent wrong answers that are not
  accommodations (they are wrong in every mode). They are filed as
  [173](../173-silent-deviations-found-by-the-strict-suite-check.md.

## Decisions (author, 2026-09-29)

- The three corpus `raise "…"` sites (`chess.py:92,128`,
  `minilight/ml/entry.py:77`) are **left as written**. They warn by default
  and are refused under `--strict`, which exercises the accommodation.
- #5: `product` yields tuples. `zip`/`map`/`filter`/`reversed` stay eager
  lists.
- #12: fixed, not flagged.
- #7: the `send` channel and `StopIteration.value` are their own issue
  ([172](../172-generator-send-channel-and-stopiteration-value.md); the
  channel is gated here meanwhile.
- #13: implement `@property` getters (dispatch per receiver class) rather
  than refuse them.

## What remains

Every row above is gated, fixed, or a documented representation. What is
left is tracked elsewhere:

- [172](../172-generator-send-channel-and-stopiteration-value.md: a typed
  `send` channel, which retires `PYC_YIELD_INT_SEND`; exception flow for
  `StopIteration.value`.
- [ifa/156](../../ifa/issues/156-FA-split-int-from-float-coerce-last.md:
  split `{int, float}` where a split exists, and coerce last (#3).
- [128](../128-cross-class-field-promotion.md steps 2-4: promoted fields as
  derived state (#8's other half).
- [007](../007-decorators-not-applied.md: property setters, and a property
  pyc cannot pre-scan.
- [173](../173-silent-deviations-found-by-the-strict-suite-check.md: silent
  wrong answers found by the verification below.

## Verification (2026-09-29)

**The pyc suite under `--strict`.** Every `tests/*.py` was compiled with
`--strict`, run, and compared with CPython's stdout and exit status (script
below). 336 matched and 38 were refused, each with a named diagnostic (type
violations, known issues, `raise "…"`, `{int, float}` without coercion). 24
differed:

- 14 are programs CPython cannot run: pyc extensions (`pyc_compat`,
  `__pyc_c_call__`) and the async runtime.
- 2 differ only in where a traceback goes (stderr vs stdout).
- 1 is `loop`'s stack overflow (above).
- The rest are silent wrong answers in every mode, filed or already
  tracked: [173](../173-silent-deviations-found-by-the-strict-suite-check.md
  (`isinstance` on `{list, str}`, a subclass's class attribute, a global
  read before definition), `zip` indexing (#5's documented
  representation), ifa/102, and 123.

```sh
# per test: CPython stdout+rc vs `pyc --strict` binary stdout+rc
ls tests/*.py | PFLAGS=--strict xargs -P 32 -n1 bash strictcmp.sh
# strictcmp.sh: copy the test to a scratch dir; run python3 (20 s cap);
# compile with `pyc $PFLAGS -D <repo>` (120 s); REFUSED if no binary,
# else MATCH / DIFFER on stdout and exit status
```

**`make test`:** green: 373 passed on both backends, 0 failed, 20 known;
`ifa --test` 58/0; `test-ir` 0 failed, 2 known. New tests:
`itertools_product_repeat_tuple`, `decorator_recursion`,
`decorator_nested_capture`, `decorator_recursion_rebind_refused`,
`decorator_dotted` (+ helper), `property_getter`,
`property_setter_refused`, `sys_exit_raises`. Eight `.check` files gained
exactly the new warnings (#3's and #7's), and nothing else.

**Corpus.** A same-day baseline was built in a worktree at `16a380f5`
(`check__default__16a380f5`):

- **Permissive** (`check__default__16a380f5+396039f2`) against that
  baseline: two verdicts changed. `kanoodle` NO → **yes** (the `sys.exit`
  fix), and `life` "compiles, then aborts" → refused. Otherwise only the
  new warnings (16 programs carry some, up from 2). CreationSets are
  2156 vs 2155.
- **Strict** (`check__PYC_STRICT_1__16a380f5+ea69f9b1`): 41 refused, 7
  fail at run time (5 CPython-length timeouts; `adatron`, ifa/102; `loop`,
  the stack, 173), 4 unverifiable, and 2 compile and print something
  different. `circle` prints `sys.version`, which cannot match.
  `tonyjpegdecoder` prints a file object's repr, `<instance>` for
  `<_io.BufferedReader name='tiger1.jpg'>`: the default-repr gap,
  [123](../123-str-does-not-fall-back-to-repr.md), wrong the same way in
  permissive. No program compiles under strict and differs because of an
  accommodation.
- **What strict refuses that permissive runs:** 16 programs. The first
  error of 14 is `has mixed basic types: (int64 float64)`, i.e. #3's
  coercion declined. `chess`'s is `raise "…"` (#1). `pisang`'s is a
  codegen guard (it aborts in permissive anyway). The coerced name is
  usually `x`, the operand formal of a builtin arithmetic method
  (mandelbrot: `temp + temp + ci`), in ant, dijkstra, fysphun,
  mandelbrot, mastermind2, neural2, sudoku2, tictactoe, timsort and
  yopyra. It is NOT yet measured which are genuinely temporal (a variable
  that starts as `0` and later holds a float, which strict is right to
  refuse) and which are one shared contour taking int actuals at some
  sites and float actuals at others. The second is an FA precision gap,
  and a split fixes it
  ([ifa/156](../../ifa/issues/156-FA-split-int-from-float-coerce-last.md)).
