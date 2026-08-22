# 117 — implicit string concatenation dropped; escaped quotes truncate

**Status:** FIXED 2026-08-21, found while clearing issues/115's residue.
Both were **silent wrong answers** — no diagnostic, plausible output.

## Symptoms

```python
a = ('ab' 'cd' 'ef')
print(len(a), a)          # CPython: 6 abcdef    pyc: 2 ab

b = 'a\'b'
print(len(b), b)          # CPython: 3 a'b       pyc: 1 a
```

## Cause

Both live in `eval_string_pyda` (python_ifa_build_if1.cc), with an
exact twin in `build_fstring_pyda`.

**1. Adjacent literals.** The grammar already accepts implicit
concatenation — `python.g`'s atom production is `STRING+`, and it
captures the raw source span covering every fragment. But the decoder
skipped one prefix, read one quote, decoded to the matching close, and
returned. Everything after the first fragment was dropped in silence.

**2. Escaped quotes.** The end-of-content scan was

```c
while (*end && *end != q && *end != '\n') end++;
```

— the first quote CHARACTER, not the first unescaped one. The tokenizer
gets this right (`shortstringsinglechar` excludes `\\` and defers to
`escapeseq`), so the token was correct and only this rescan was wrong.

The two compound: a truncated literal leaves the rest of the token
looking like the start of another one, so fixing concatenation without
fixing escapes would have turned a truncation into a misparse.

## Fix

One escape-aware scanner, `scan_string_literal`, returns a single
fragment's content range and its own prefix flags and points past the
closing quote; `skip_between_string_literals` steps over the whitespace,
newlines, line-continuations and comments that sit between fragments in
the raw span. `eval_string_pyda` loops over fragments and folds them
into one constant at compile time (they are all literals — no `__add__`
sends). `build_fstring_pyda` loops over the same sequence, treating a
non-`f` fragment as a plain piece, so `f'a={a}' ' plain'` works in
either order — and `PY_string`'s dispatch now checks whether ANY
fragment carries an `f` prefix, not just the first.

Escapes suppress the closing quote in a RAW string too (`r'a\'b'` is one
literal, four characters); raw-ness changes how the content DECODES, not
where it ends.

## What this blocked

`shedskin_examples/sunfish` writes its board as twelve adjacent
literals with a trailing comment on each row, so `initial` was 10
characters instead of 120 and every board scan found nothing. Nothing
warned; the program just reported no legal moves.

More broadly: implicit concatenation is how essentially all Python code
writes long strings, so any corpus program using it was silently
compiling a truncated string.

## Verification

`tests/string_literal_concat.py` — concatenation (inline, parenthesized
multi-line, with interleaved comments, with line continuations),
escaped quotes in both quote styles, raw strings, triple-quoted
fragments, bytes fragments, and f-strings in either position. Both
backends 288 -> 292 passed / 0 failed.
