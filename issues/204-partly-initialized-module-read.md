# 204 — a circular import's read of a name not yet bound sees the later value

**Status:** open. Filed 2026-11-20, found when circular imports stopped
crashing the compiler ([202](202-os-and-pathlib.md)). A wrong answer, but
only for a program that CPython stops with an exception.

## Symptom

```python
# partinit_pkg/__init__.py
from . import sub
VAL = 5
# partinit_pkg/sub.py
import partinit_pkg
try:
    print(partinit_pkg.VAL)       # CPython: AttributeError (partially initialized module)
except AttributeError:
    print("AttributeError")
```

pyc prints `5`.

## Mechanism

A module's names are bound before its body runs as far as the analysis is
concerned: `VAL`'s only binding is the constant 5, so the read is folded
(or reads the global the later store fills). CPython runs `sub` while
`partinit_pkg`'s body is stopped at its `from . import sub`, so `VAL` does
not exist yet.

## Plan

Refuse at compile time a module-level read of `M.x` that runs during the
import of `M` (directly, or from a function the importing module's body
calls during it) when `x`'s binding in `M` comes after the import that is
in progress. Reads inside functions called later are the normal case and
stay as they are. Low priority: it changes only programs that fail in
CPython.

## Tests

`tests/import_partly_initialized.py` (fixture `tests/partinit_pkg/`) with
a `.known_issue` sidecar.
