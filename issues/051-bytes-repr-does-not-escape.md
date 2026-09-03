# 051 — `repr(bytes)` does not escape non-printable bytes

**Status:** open, 2026-08-15. Found while pinning the semantics of
`bytes(list)` for [050](050-pyc-string-builders-are-quadratic.md). Repro
landed as `tests/bytes_repr_escapes.py` with a `.known_issue` tag.

## Symptom

```python
print(bytes([0, 255]))
```

CPython prints `b'\x00\xff'`. pyc emits the **raw bytes** — a NUL and a
0xff — inside the `b'...'` quotes, so the output is not a valid `repr`
and does not round-trip. It also makes the output binary rather than
text, which is why the test harness reports the diff as "Binary files
differ" rather than showing it.

## Scope

Any non-printable or non-ASCII byte: control characters, `\t` / `\n` /
`\r` (which CPython renders as `\t`, `\n`, `\r`), and anything ≥ 0x80.
Printable ASCII is already correct — `bytes([65, 66, 67])` gives
`b'ABC'` on both.

## Where to look

`bytes.__repr__` / `__str__` in `__pyc__/01b_bytes.py`, which needs the
same escaping table CPython uses: `\\`, `\'`, `\t`, `\n`, `\r`, and
`\xNN` for everything outside `0x20..0x7e`.

## Verification plan

- `tests/bytes_repr_escapes.py` passes and its `.known_issue` tag is
  deleted.
- `tests/bytes_from_list.py` keeps passing (it deliberately avoids
  non-printables so the two concerns stay separable).

## What this unblocks

Nothing is blocked, but any program that prints a `bytes` containing
binary data currently produces output that differs from CPython — and
because the difference is *binary*, a diff-based test looks unhelpful
until you know to look for this.

## 2026-09-03: `repr(str)` has it too, not just `repr(bytes)`

Found alongside [124](124-crlf-source-not-newline-normalized.md). The
title says `bytes`, but plain `str` is identical:

```python
print(repr("a\nb"))
print(repr("x\ty"))
```

| | output |
|---|---|
| CPython | `'a\nb'` / `'x\ty'` |
| **pyc**  | a literal newline / a literal tab inside the quotes |

So the escaping gap is in the shared repr path, not something specific
to `bytes`. Whatever fix lands should cover both, and the verification
should check `str` as well as `bytes` — `tests/bytes_repr_escapes.py`
currently pins only the `bytes` half.

Worth noting it is invisible in most corpus programs because `repr` of a
control-character-bearing string is rare, but it makes `repr` unusable
for the thing it exists for: an unambiguous rendering.
