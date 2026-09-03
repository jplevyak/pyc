# 124 — a CRLF source file puts `\r\n` inside string literals

**Status:** open, filed 2026-09-03. Found while converting the corpus's
CRLF files to LF — the conversion would have silently MASKED this.
**Area:** pyc frontend (`python_parse.cc` / the dparser tokenizer's
source reading).
**Severity:** **silent** — zero warnings, exit 0, wrong string contents
and wrong `len()`.
**Reproducer:** `issues/repro/124-crlf-source-newline.py` (4 lines, and
the file itself must keep CRLF endings — see "Why the repro is not in
`tests/`").

## Symptom

```python
a = """one
two"""
print(len(a))
print(a == "one\ntwo")
```

saved with **CRLF** line endings:

| | `len(a)` | `a == "one\ntwo"` |
|---|---|---|
| CPython | `7` | `True` |
| **pyc** | **`8`** | **`False`** |

No warnings, exit 0.

## Cause

CPython reads source with **universal newlines** (PEP 278): `\r\n` in the
file becomes `\n` before the tokenizer ever sees it, so a triple-quoted
string spanning lines contains `\n`. pyc reads the bytes as-is, so the
literal keeps `\r\n` — one extra character per line, and any comparison
against a `\n`-containing string fails.

This affects only string literals that SPAN LINES (triple-quoted, or
implicit concatenation across a continuation); single-line literals are
unaffected because no newline is captured.

## Scope in this repo

`tests/` is entirely LF (0 of 345 files). The corpus had 20 CRLF files,
of which 7 contain multi-line string literals, i.e. were actually
mis-parsed:

    bh/bh.py                      27 multi-line literals (docstrings)
    circle/circle_main.py          1
    mao/mao.py                     1
    minilight/ml/entry.py          4  (banner/usage text — printed)
    rubik/rubik.py                 1
    tonyjpegdecoder/….py          12
    webserver/webserver.py         5  (HTTP response templates — printed)

`webserver` and `minilight` PRINT theirs, so pyc was emitting `\r\n`
where CPython emits `\n`. Neither is caught by the corpus sweep today
because both are in `run_fail` (rc=124 / rc=134), so their stdout is
never compared.

Those 20 files were converted to LF in the same change that filed this,
which makes the corpus consistent and incidentally stops exercising this
bug. **The bug is unfixed** — it just no longer has a witness in-tree,
which is exactly why it is filed here with its own repro.

## Why the repro is not in `tests/`

A regression test for this has to BE a CRLF file, and that is fragile in
a way worth stating: any future line-ending normalization (a
`.gitattributes` rule, `core.autocrlf`, an editor, a bulk `dos2unix`)
would silently convert it and retire the coverage without failing
anything. The repo has no `.gitattributes` and `core.autocrlf` is unset
today, so nothing normalizes right now — but a test that quietly stops
testing is worse than none.

If this is fixed, the right pinning is a test that WRITES a CRLF file
and compiles it, rather than one that ships as a CRLF file.

## Fix

Normalize `\r\n` (and lone `\r`) to `\n` when reading the source buffer,
before tokenizing — matching CPython's universal-newline behaviour.
Doing it at read time rather than in the string-literal path also covers
line numbering and any other position arithmetic.

## Verification plan

1. The repro prints `7` and `True`.
2. A CRLF file with a single-line string literal is unchanged (it
   already works — do not regress it).
3. Re-converting one corpus file to CRLF and compiling it produces
   output identical to the LF version.

## What this unblocks

Correctness for any Python source not authored on Unix. It is a
whole-class silent divergence: nothing in the compile output hints at
it, and the only corpus programs that exercised it were ones whose
stdout the sweep never checks.
