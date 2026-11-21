# 203 — a str's length and indexing count UTF-8 bytes, not characters

**Status:** open. Filed 2026-11-20, found writing
[202](202-os-and-pathlib.md)'s test of `write()`'s return value. A silent
wrong answer on any non-ASCII text.

## Symptom

```python
s = "héllo 世界"
print(len(s), s[1], s[-1], s[1:3], s[::-1])
#  CPython: 8 é 界 él 界世 olléh
#  pyc:     13, then single bytes of é and 界 and byte-reversed text (invalid UTF-8)
print([c for c in "été"], "été".find("t"), "naïve".upper())
#  CPython: ['é', 't', 'é'] 1 NAÏVE
#  pyc:     five one-byte strs, 2, NAïVE
```

Every operation that counts or indexes: `len`, `s[i]`, slicing, iteration,
`find`/`index` results, `ljust`/`center` widths, `%5s` and `{:5}` padding,
`write()`'s return value, and `upper`/`lower` of non-ASCII letters.

## Mechanism

A pyc str is a length-prefixed byte buffer (RUNTIME.md, string layout),
and source text and file contents arrive as UTF-8 bytes. Every str
operation works on bytes. Nothing marks the deviation: no warning, and
`--strict` compiles it.

## Plan (a decision first)

- **Keep UTF-8, count code points.** `len` and indexing become O(n) unless
  the str carries an "all ASCII" bit (or a code-point count and a stride
  index), set where it is made. ASCII text, nearly all of the corpus,
  stays O(1). PyPy does this.
- **CPython's flexible representation** (1, 2 or 4 bytes per character,
  chosen at construction). O(1) indexing everywhere, but every boundary
  with C (`print`, files, `__pyc_c_call__` with `str`) needs an encode.
- **Refuse it** under `--strict` at least: a str literal or a file read
  with a non-ASCII byte is a compile-time or run-time refusal. Honest and
  cheap, but files are only known at run time.

## Tests

`tests/str_non_ascii.py` with a `.known_issue` sidecar.
