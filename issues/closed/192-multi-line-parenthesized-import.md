# 192 — `from m import (...)` across lines did not parse

**Status:** closed 2026-11-19, the day it was filed. Found compiling
shedskin's own source, where `config.py`, `cpp.py`, `infer.py` and others
import this way. Noted earlier while writing the ctypes tests (issues/188),
which split their imports instead.

## Symptom

```python
from typing import (
    IO,
    Any,
)
# pyc: syntax error after '('
```

## Cause

`import_from` in `python.g` matched the parentheses as the literal tokens
`'('` and `')'`. Every other bracket goes through `LP` / `RP`, whose actions
count `implicit_line_joining`, so a newline inside them is not a statement
end. Inside the import's parentheses it was.

## Fix

`'(' import_as_names ')'` is now `LP import_as_names RP`.

## Tests

`tests/import_parenthesized.py`: the names across lines, with an
end-of-line comment, a whole-line comment, `as`, and a trailing comma, plus
a one-line parenthesised import. Matches CPython on both backends and under
`--strict`.
