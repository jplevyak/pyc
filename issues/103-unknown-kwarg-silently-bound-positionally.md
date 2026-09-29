# 103 — a refused program is reported in the analyzer's terms, not as the user's error

**Status:** open. Rewritten 2026-09-28; it now also carries
[107](closed/107-undefined-names-warn-then-segfault.md). History in git:
`git show e3b44e2c:issues/103-unknown-kwarg-silently-bound-positionally.md`.

## Where it stands

Both original defects are no longer silent. An unknown keyword argument
is no longer bound to the next positional parameter (`PYC_KWSTRICT`,
default on, rejects the candidate at `pattern.cc` after
`find_all_matches`). An undefined name no longer compiles and segfaults
(`c8d7da8d`). Since closed ifa/158 made every violation fatal, both
REFUSE the program. What is wrong now is **what they say**
(re-verified 2026-09-28):

| program | CPython | pyc |
| --- | --- | --- |
| `f([1, 2], nosuchkw=99)` | `TypeError: f() got an unexpected keyword argument 'nosuchkw'` | `error: illegal call argument type expression illegal: f` |
| `print(NoSuchName)` | `NameError: name 'NoSuchName' is not defined` | `error: 'NoSuchName' has no type` / `expression has no type` |

`x = NoSuchName` and a name inside a `def` body already say
`name 'NoSuchName' is not defined`. The gap is a name used as a CALL
ARGUMENT: that position does not record a pending use for
`report_undefined_names`.

Fixtures: `tests/unknown_kwarg_rejected.py` and
`tests/undefined_name_executed.py` (both `.known_issue`, with checks that
hold the correct diagnostic).

## Fix

1. **Undefined name as an argument:** record the pending use in the
   argument-lowering path, the way an ordinary load does, so
   `report_undefined_names` reports it before FA runs.
2. **Unknown keyword:** when the candidate is rejected for an unmatched
   name, report it at the call site in CPython's words, naming the
   function and the keyword. It should not fall through to a generic
   "illegal call argument type".
3. **General rule:** a refusal names the user-level cause in CPython's
   terms. `'X' has no type` describes analyzer state. It should appear
   only when there is no better cause to report, and never as the first
   line when a named cause exists.

## Verification

Both fixtures print their `.check` diagnostics; delete their
`.known_issue` sidecars. `life` and `itertools_module` stay green
(`product(repeat=)` landed with this issue).
