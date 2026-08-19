# 113 — package imports

**Status:** implemented (`bd78f917`); follow-ups below
**Area:** pyc frontend (module resolution, grammar)
**Test:** `tests/import_package.py` + `tests/pyc_pkg/`

## What was missing

Module resolution was `dupstrs(root, "/", mod, ".py")` — the module
name pasted in verbatim. So:

- a **package** (directory + `__init__.py`) was never considered
- a **dotted** name was taken literally: `com.github.tarsa.tarsalzp.Main`
  looked for a file named `com.github.tarsa.tarsalzp.Main.py`
- **relative** imports lost their dots in the grammar, so
  `from .camera import Camera` was resolved as absolute `camera`

Plain sibling-module imports always worked (`import helper`,
`from helper import greet`), and eight of the corpus programs that
compile use them — this was specifically about packages.

## Diagnostic gap

None of the three corpus programs reported an import problem. They
reported the *consequence*:

```
error line 11, name 'entry' is not defined
fail: 1 undefined name
```

The failed import was silently dropped and the undefined-name check
(issues/107) reported the downstream symptom. Synthetic reproductions
of the same shapes *did* report `cannot find module`, so the two paths
diverge somewhere that was not isolated. Worth chasing: a user whose
package import fails should be told that, not that a name is undefined.

## Corpus effect

    before   66 compiled, 11 failed of 77
    after    67 compiled, 10 failed of 77

| program | before | after |
|---|---|---|
| minilight | `name 'entry' is not defined` | **compiles** (crashes at runtime) |
| quameon | `name 'simple_jastrow' is not defined` | typing error in `stats/average.py` |
| tarsalzp | `name 'Main' is not defined` | typing error in `core/Lg2.py` |

minilight compiling is not minilight working — it aborts with "getter
not resolved", the corpus's usual compile-then-crash pattern. quameon
and tarsalzp now fail on ordinary type inference deep inside their
packages, which is the point: the module layer is no longer the
blocker.

## Two bugs found on the way

- **dparse rejected empty files.** `buf_read` returns -1 only when
  `open()` fails; 0 is a successful read of an empty file and it still
  returns a NUL-terminated buffer. The `<= 0` check rejected every
  empty `__init__.py` — the common case for a package.
- **`MemoryError` was missing** from `__pyc__/08_exception.py`.

## Follow-ups

1. **The diagnostic gap above** — silent drop instead of a module
   error.
2. **`import a.b` binds only the top name.** `build_import_syms` keeps
   its old fallback of resolving `a.b` to `a`, matching CPython's
   binding for that form, but now that packages resolve properly this
   should bind the real submodule and let `a.b.c` attribute chains
   work.
3. **No `__init__.py` re-export semantics beyond plain names.** A
   package's `__init__` that does `from .x import y` re-exports `y`
   because symbol building runs it; anything computed (`__all__`,
   conditional imports) is not modelled.
4. **`from . import x`** (bare dot, no name) resolves the package but
   is not exercised by any test or corpus program.
