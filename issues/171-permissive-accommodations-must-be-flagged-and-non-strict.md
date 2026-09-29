# 171 — every Python accommodation must be flagged, and off under `--strict`

**Status:** open. Filed 2026-09-28 from the author's directive:
*"all python specific features are under flags … all such accommodations
should be in non-strict mode."* This is the audit, with a verdict per
item. The policy it enforces is in AGENTS.md ("Generalised … every
Python-specific accommodation is flagged and non-strict") and, for corpus
programs, in [shedskin_examples/PYC_CHANGES.md](../shedskin_examples/PYC_CHANGES.md).

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
| 3 | `{int, float}` widened to `float` (prints `1.0` for `1`) | `fa_coerce_numeric_confluences` / `coerce_annotate` | `fruntime_errors` (ifa/145) ✓ | ✓ gated. ✗ **silent:** only `PYC_DBG_NUMC` shows it. Add a warning naming the variable. Prefer a split where one exists ([ifa/156](../ifa/issues/156-FA-split-int-from-float-coerce-last.md)). |
| 4 | `tuple(iterable)` returns a **list** | `python_ifa_build_if1.cc:958` | **DONE 2026-09-28:** `PYC_MAKESEQ=1` is the default, so `tuple(x)` is a real runtime-length tuple in every mode (`PYC_MAKESEQ=0` restores the list) | ✓ Nothing left to gate. The blocker was tuple deepcopy ([110](110-tuple-from-iterable-returns-a-list.md)). |
| 5 | `zip`/`map`/`filter`/`reversed` return eager lists; `itertools.product(repeat=)` yields lists | `__pyc__/05_builtins.py:222`, `pyc_lib/itertools.py:29` | none | Mostly unobservable: equivalent under iteration and `list()`. Observable when the loop mutates the source, on an infinite iterable, or when the result is printed or hashed. **Decide per case:** the `product` tuple-vs-list difference is observable and belongs with #4. |
| 6 | `None` read as the zero value (`None + "y"` gives `"y"`) | nullable-pointer representation (ifa/164) | **DONE 2026-09-28** in every mode: a `{None, T*}` receiver is checked where the call is emitted and reports CPython's uncaught `TypeError` (`_CG_none_receiver`), and `{None, scalar}` is refused at FA time (a BOXING violation) | ✓ `tests/none_receiver_raises.py`. Remaining: the check reports and exits, so `except TypeError` cannot catch it ([ifa/165](../ifa/issues/165-none-reaching-an-operation-is-silently-accepted.md)). |
| 7 | a generator's return / `send` value smuggled through `int64` (`0` where CPython gives `None`) | `__pyc__/09_generator.py`, `08_exception.py` | **RETURN value DONE 2026-09-28**, every mode | ✓ The return value has its own channel: the body stores `return X` in its own `__pyc_generator__` through a hidden formal (`gen_retcell`), so it is typed per generator and is `None` when there is no `return X`. `yield from` asks the sub-generator for it. That fixed `yield "q"; return "s"` printing an address. **Open:** (a) `except StopIteration as e: e.value` reads the one global exception slot, which unions every StopIteration in the program, so it is refused when two value types meet (`tests/stopiteration_value_exception_slot.py`, `.known_issue`; the fix is exception flow along the call graph); (b) the `send` channel is still `int`: a non-int `send` is refused, and `x = yield` resumed by `next()` reads `0` for CPython's `None`. |
| 8 | "a write to a missing field discovers the field" | `P_prim_setter` in generic `ifa/analysis/fa.cc` | none: hard-coded in ifa | This is Python semantics, not a deviation, but it is Python-specific. ✗ Move it behind `IFACallbacks::discovers_fields_by_write()` ([128](128-cross-class-field-promotion.md)). |
| 9 | locals possibly read before assignment are zero-filled | `--safe` (ifa/039) | `--safe` only ✓ | ✓ already a flag, and non-strict by construction. |
| 10 | a type violation compiled with a runtime check instead of refused | `fruntime_errors`, `convert_NOTYPE_to_void` | ✓ gated. `PYC_STRICTVIOL=1` (closed ifa/158) currently makes every violation fatal in every mode anyway | ✓ consistent. Note that permissive currently refuses too. |
| 11 | `range` is its own iterator (`__iter__` returns `self`), so a range iterates once; CPython's is re-iterable | `__pyc__/05_builtins.py` | none | ✗ Not an accommodation anyone needs. Make `range.__iter__` return a fresh iterator ([125](125-in-has-no-iterable-fallback.md)). |
| 12 | a recursive call inside a decorated function calls the UNDECORATED function | `def_internal_fn` (`python_ifa_int.h`) | none | ✗ Fix, or refuse under strict ([007](007-decorators-not-applied.md)). |
| 13 | an unhandled decorator (`@property` with a computed getter, dotted names) is ignored | `PY_decorated` | none | ✗ Not an accommodation: a silent wrong answer. Refuse in every mode ([007](007-decorators-not-applied.md)). |

Accommodations that are **representations, not deviations**, and so are
not listed: `{None, T*}` as a nullable pointer (CPython-faithful while the
`None` is never read), and arity and prefix layouts.

**Found 2026-09-28, pre-existing (same on `5025025e`):** `--strict`
refuses EVERY generator, even `def g(): yield 1`, with "unable to resolve
to a single function at call site". It refuses rather than miscompiles,
but strict cannot compile a program that uses a generator at all. Root-
cause it before anything else under strict.

## Order of work (updated 2026-09-28)

1. ~~**#2**, the inversion~~ and ~~**#1**, gating plus a warning~~:
   DONE 2026-09-28. Still to decide: the three corpus `raise "…"` sites.
   The recommendation is to edit them (`raise Exception("…")`): they are
   actual type errors, and the edit costs nothing on the paths CPython
   runs. ~~Make the C backend's `{None, scalar}` message match LLVM's~~:
   DONE 2026-09-28. Both backends now refuse it at FA time with the same
   diagnostic.
2. ~~**#6** via ifa/165~~ and ~~**#4** via 110~~: DONE 2026-09-28. ~~**#7**'s
   return value~~: DONE; its `send` channel and the exception-slot read
   remain.
3. **#3**: add the warning.
4. **#8**: the `IFACallbacks` hook (mechanical, no behaviour change for
   pyc).
5. **#5**, and the strict-generator refusal above.

## Verification

- `--strict` on the pyc suite: every test whose output differs from
  CPython is either refused with a named diagnostic or listed here with a
  plan. Strict must never compile it silently.
- A strict-mode corpus sweep (`./corpus_sweep.sh -m check -e
  "PYC_STRICT=1"`): every program either matches CPython or is refused.
  None may compile and print something different.
- The default (permissive) sweep: unchanged except for the new warnings.
