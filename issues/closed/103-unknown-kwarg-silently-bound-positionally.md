# 103 — a refused program is reported in the analyzer's terms, not as the user's error

**Closed 2026-10-14.** Both diagnostics now say what CPython says, as
compile errors; see "Resolution" at the end.

**Status at the time (kept as the record):** open. Rewritten 2026-09-28; it now also carries
[107](107-undefined-names-warn-then-segfault.md). History in git:
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

## Resolution (2026-10-14)

- **Undefined name as a call argument.** `build_syms_pyda` grouped
  `PY_arglist` (and `PY_list`, `PY_slice`, `PY_subscriptlist`, the
  literals) with `PY_keyword_arg`, whose child 0 is a parameter NAME and
  is walked with `in_kwarg_key` set so it is never reported. For those
  kinds child 0 is an ordinary expression -- the first argument of every
  call, the first element of every list -- so an undefined name there was
  never recorded. They now walk every child normally; only
  `PY_keyword_arg` and the parameter-declaration kinds keep the
  suppression. `report_undefined_names` also uses the standard
  `file:line: error:` form: `undefined_name_executed.py:11: error: name
  'NoSuchName' is not defined`.
- **Unknown keyword argument.** When `PYC_KWSTRICT` rejects every
  candidate of a call for a keyword that names no formal, `pattern.cc`
  records the callee and the keyword against the send
  (`unexpected_keyword_at`), and the call's type violation is reported in
  CPython's words: `unknown_kwarg_rejected.py:29:44: error: f() got an
  unexpected keyword argument 'nosuchkw'`.

Both fixtures are `.check_fail` tests now, their checks the actual
diagnostics.
