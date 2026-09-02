# 108 — `async with … as X` does not bind `X`

**Status: FIXED 2026-08-18**, same day, as part of
[107](../107-undefined-names-warn-then-segfault.md). `PY_with_item` now
marks its `as` target `PY_STORE`, exactly as assignment targets and `for`
variables are marked.

**The scope was wider than this title says:** plain `with … as X` did not
bind `X` either. It only *looked* async-specific because the first
version of 107's check tracked bindings flatly, so a same-named binding
anywhere in the file masked it. With scope-aware tracking, `with_basic.py`
failed too — and one fix resolved both.

**Status:** open, found 2026-08-18 when
[107](../107-undefined-names-warn-then-segfault.md)'s undefined-name check
made it visible. Repro: `tests/async_syntax.py` (`.known_issue`).

## Symptom

```python
async def test_with():
    async with my_context() as ctx:
        print(ctx)          # error line 23, name 'ctx' is not defined
```

`ctx` is never bound, so the reference falls through to an unresolved
global. Before 107 that silently minted a never-assigned symbol; now it
is reported.

## Scope — narrow

Both neighbouring constructs are fine:

| construct | binds? |
|---|---|
| `with C() as x:` at module level | ✅ |
| `with C() as x:` inside a function | ✅ |
| `async for i in it():` | ✅ (`i` binds) |
| **`async with c() as ctx:`** | ❌ |

So it is specific to the `as` target of an **async** `with`, not to
`with` or to async in general. `python.g` uses the *same* `with_item`
rule for both (`async_with_stmt: 'async' with_stmt`, setting only
`is_async`), and `build_if1`'s `PY_with_stmt` lowering is shared — so the
divergence is in how `build_syms` walks the async-marked statement.

## Where to look

`python_ifa_build_syms.cc`'s `PY_with_stmt` / `PY_with_item` case does a
plain `generic_recurse` and never marks the `as` target `PY_STORE`, the
way assignment targets and `for` variables are marked. The sync path
evidently binds the name by another route that the async path misses;
find that route, or mark the target explicitly in the `with_item` case.

## Verification plan

- `tests/async_syntax.py` compiles without an undefined-name error;
  delete its `.known_issue` tag.
- `with … as x` keeps working at module and function scope, sync and
  async, and the bound value is correct at runtime (not merely bound).
