# 205 — `textwrap`, and the str and `del` gaps it found

**Status:** open for its residuals. Landed 2026-11-20, the third of the
widely used modules chosen in [196](196-compiling-shedskin-with-pyc.md)
(shedskin's `cpp.py` calls `textwrap.dedent`), after
[202](202-os-and-pathlib.md).

## What is there

`pyc_lib/textwrap.py`, a port of CPython 3.12's: `TextWrapper` with all
its options (`width`, both indents, `expand_tabs`, `tabsize`,
`replace_whitespace`, `fix_sentence_endings`, `break_long_words`,
`break_on_hyphens`, `drop_whitespace`, `max_lines`, `placeholder`),
`wrap`, `fill`, `shorten`, `dedent` and `indent`. CPython chunks the text
with one regex (`wordsep_re`); the port's `_split_hyphenated` is that
regex's matcher written out alternative by alternative, lookbehinds and
the lazy word match included, so the chunks and therefore the lines are
CPython's. `tests/textwrap_basic.py` compares 126 lines of output over
eleven texts and every option, on both backends and under `--strict`.

## Not the same

- `wrap()`, `fill()` and `shorten()` take TextWrapper's options as named
  parameters, not `**kwargs`: forwarding `**kwargs` whose values have
  different types is refused ([189](189-unpacking-into-named-parameters.md)).
  Every call CPython accepts by keyword is accepted.
- Widths count UTF-8 bytes, so non-ASCII text wraps early, and a
  non-ASCII letter is not a letter to the hyphenation rule
  ([203](203-str-counts-utf8-bytes.md)).

## Found and fixed on the way

- **`del l[-1]` deleted nothing.** `list.__delitem__(i)` deleted the
  slice `[i:i+1]`, which is empty for `-1`. A silent wrong answer in any
  program deleting from the end; textwrap's line loop hung on it. A
  negative index now counts from the end.
- **`del l[i]` out of range and `del d[k]` for a missing key did nothing.**
  They raise `IndexError` and `KeyError` now, as CPython's do, and the
  `del` statement is followed by the exception check a call gets (an
  item READ still is not, [175](175-an-exception-raised-in-an-operator-method-does-not-propagate.md)),
  and arms it: a program whose only possible exception is a `del` now
  stops with it instead of carrying on.
- **str had no `isalpha`, `isalnum`, `isdecimal`, `isnumeric`,
  `isascii`, `isidentifier`, `isprintable`, `istitle` or `expandtabs`**,
  and `splitlines` split only on `\n` and `\r` and had no `keepends`. All
  added, CPython's for ASCII. A non-ASCII character is none of the
  classes (203).

## Corpus

`check__default__020ede00+4ce3adec` matches
`check__default__5ad2dc46+f9726c92` on every verdict (77 compile, none fail
to run, only othello2's stdout differs, issues/177). Container
CreationSets are unchanged; 20 programs that use `del` gain a few
contours for its raise and check (+60 EntrySets and +248 CreationSets
corpus-wide, linalg the most at +8/+42). Compile time summed 1210 s
against 1194 s.

## Tests

`textwrap_basic`, `str_classify_lines`, `del_item` (both backends and
`--strict`); `del_item_uncaught` checks the uncaught `IndexError`.
