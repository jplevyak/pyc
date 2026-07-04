# Issue 022: Implement `async`/`await` (PEP 492)

**Status:** open.
**Affects:** `python.g` and pyc lowering.

## Description

```python
async def foo():
    return 1
```
```
async_test.py:1: syntax error after 'async'
```
No `async`/`await` keyword or rule anywhere in `python.g`.
Implementing this meaningfully requires an event-loop/coroutine runtime model — likely the largest remaining syntax gap, probably larger in scope than issue 014's generators (which it depends on/relates to, since `async def` bodies are generator-like).

## Verification plan
1. async/await: minimal `async def` + `await` round-trip (largest lift; may warrant its own design doc before implementation, similar to issue 014).
2. Add one test file for async/await once implemented.
