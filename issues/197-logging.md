# 197 — `logging`

**Status:** open for its residuals. `pyc_lib/logging.py` landed 2026-11-19,
the first of the widely used modules chosen in
[196](196-compiling-shedskin-with-pyc.md) (shedskin imports it in seven
modules).

## What is there

A port of the core of CPython's `logging/__init__.py`. For what it covers,
the output is CPython's byte for byte:

- **Levels.** The constants, `getLevelName`, `addLevelName`, and a level
  given by name (`setLevel("DEBUG")`, `basicConfig(level="INFO")`) with
  CPython's `ValueError` for an unknown one.
- **`LogRecord`.** `getMessage()` applies `msg % args` when a handler
  formats it, not when the call is made, as CPython does. `created`, `msecs`
  and `relativeCreated` are real.
- **`Formatter`.** `%` style, any `%(field)spec` over `name`, `levelname`,
  `levelno`, `message`, `asctime`, `created`, `msecs` and `relativeCreated`.
  `formatTime` uses the default time format, with `,msecs`, or a `datefmt`.
  `%%` is a literal `%`. A format with no field is CPython's `ValueError`.
  Subclasses that override `format` work.
- **`Filter`** (CPython's name-prefix rule), and `Filter` subclasses on
  loggers and handlers.
- **Handlers.** `Handler` (subclass and override `emit`), `StreamHandler`
  (default `sys.stderr`, or any object with `write` and `flush`, e.g.
  `io.StringIO`), `FileHandler`, `NullHandler`, and `lastResort` for a
  logger with no handlers anywhere.
- **Loggers.** `getLogger` with the dotted hierarchy, including a child made
  before its parent (CPython's placeholder re-parenting), `propagate`,
  `getEffectiveLevel`, `isEnabledFor`, `getChild`, `hasHandlers`,
  `disable()`, `log(level, ...)`, and the module-level `debug` … `critical`,
  which call `basicConfig()` first as CPython's do.
- **`basicConfig`** with `filename`, `filemode`, `format`, `datefmt`,
  `style`, `level`, `stream`, `handlers` and `force`, and CPython's
  `ValueError`s for conflicting arguments.

Two runtime additions came with it: `time.strftime(format)` (current time
only) and `_CG_strftime_local` behind both.

## Refused

Each of these is a compile error (an unknown name, member or keyword) or a
`NotImplementedError` raised before anything prints:

| what | why |
| --- | --- |
| `%(lineno)d`, `%(funcName)s`, `%(pathname)s`, `%(filename)s`, `%(module)s`, `%(process)d`, `%(thread)d`, … | CPython reads them from the caller's stack frame, which pyc does not have. Refused when the `Formatter` is made |
| `Logger.exception()`, `exc_info=`, `stack_info=` | they need the traceback |
| `extra=`, `stacklevel=` | not ported |
| `warn()` | a `DeprecationWarning` in CPython, which pyc does not model |
| the `{` and `$` styles | not ported |
| a dict as the only message argument (`log.info("%(x)s", d)`) | it changes `args` to the dict, a second type for one field |
| `getLevelName("INFO")` (a name back to its number) | the answer's type differs from the other direction's |
| a function as a filter | only `Filter` objects |
| `LoggerAdapter`, `logging.handlers`, `logging.config` | not ported |
| `FileHandler(delay=True)` | not ported |

## Deviations

- **On stderr only:** a handler whose format raises prints CPython's
  `--- Logging error ---` report without the traceback, then the message and
  arguments, and carries on as CPython does.
- `Filterer.filter` returns a bool, as CPython 3.11's does. 3.12's returns the
  record.

## Found and fixed on the way

Writing it found five bugs in pyc itself:

- [198](closed/198-base-method-call-through-a-wrapper-reaches-the-override.md):
  `Base.m(self)` or `super().m()` that needed a wrapper (a default, keywords
  out of order, `*args`, `f(*t)`) called the receiver's override instead.
  `Handler.__init__(self)` from `StreamHandler.__init__(self, stream=None)`
  was the case.
- [199](closed/199-percent-r-in-a-computed-format.md): a computed format's
  `%r` printed `str()`, not `repr()`. Every logging call formats its message
  through a computed format.
- `str.rfind` and `str.rindex` did not exist (logging's hierarchy uses
  `rfind` with bounds).
- [ifa/213](../ifa/issues/213-slot-elision-truth-test-on-a-subclass-union.md):
  a truth test on `{None, Base, Sub}` makes codegen refuse the program. The
  shim tests `is not None` where CPython's source tests truth (the same answer
  for these classes, which define no `__bool__`). A user program that tests a
  logger or handler for truth still hits it.
- Two more filed and not fixed:
  [200](200-percent-d-of-a-str-prints-a-pointer.md) and
  [201](201-list-layout-tuple-at-a-computed-format-segfaults.md).

## Tests

`tests/logging_basic.py`, `tests/logging_handlers.py`,
`tests/logging_hierarchy.py` and `tests/logging_defaults.py` (all on stderr)
match CPython on both backends and under `--strict`.
`tests/logging_exception_refused.py` and
`tests/logging_frame_field_refused.py` check the refusals.

Each logging test takes 4 to 10 s to compile, mostly flow analysis over the
module's class hierarchy (6 s of `logging_basic`'s 10). The handler tests are
split in two because one file at 10 s timed out under the suite's parallel,
double-compiling run.

Corpus: `check__default__ce537989+8714296f` matches
`check__default__d3c93d16+35a20895` on every verdict (77 compile, none fail to
run, stdout as before: only othello2 differs, issues/177). CreationSets rise
by 1 to 3 per program: the constants of 199's per-argument conversion.
Container CreationSets are unchanged everywhere. mastermind2, the one corpus
program with a computed format (`fmt % i`), gains 11 EntrySets and 17
CreationSets for that conversion, and its output still matches. chaos's run
timeout in the earlier sweep is gone (load).
