# 210 — `re`: findall, finditer, sub, split, named groups, flags

**Status:** open for its residuals. Landed 2026-11-24. `pyc_lib/re.py`
had a parser and a backtracking matcher but only `compile`, `match`,
`fullmatch` and `search`, so a program calling `re.sub` or `re.findall`
did not compile. It is now the module CPython documents, minus what is
listed under "Not the same", and every result matches CPython 3.12 on both
backends and under `--strict`.

## What is there

- **Module functions**: `compile`, `search`, `match`, `fullmatch`,
  `findall`, `finditer` (a generator, lazy as CPython's iterator is),
  `sub`, `subn`, `split` (with `maxsplit`, and the groups of the separator
  in the result), `escape` (3.7's character set), `purge`, `error`. A
  compiled pattern may be passed where a pattern string is. Compiled
  patterns are cached by (flags, pattern), as CPython's are.
- **Pattern**: the same methods with `pos` and `endpos`; `pattern`,
  `flags` (with `UNICODE` added for a str pattern, as CPython does),
  `groups`, `groupindex`, and CPython's repr.
- **Match**: `group` (by index or name; `None` for a group that did not
  take part), `m[g]`, `groups(default)` and `groupdict(default)` (a real
  tuple and dict), `start`/`end`/`span` by index or name, `expand`,
  `string`, `re`, `pos`, `endpos`, `lastindex`, `lastgroup`, and CPython's
  repr.
- **Replacement templates**: `\1`, `\g<1>`, `\g<name>`, `\g<0>`, octal
  escapes, `\n` `\t` and the rest; an unknown ASCII-letter escape is an
  error and any other is kept with its backslash; a group that did not take
  part is empty. A function replacement is called with the Match.
- **Syntax**: everything the old engine had, plus `(?P<name>...)`,
  `(?P=name)`, `\1`-`\99`, lookahead `(?=...)` `(?!...)`, fixed-width
  lookbehind `(?<=...)` `(?<!...)`, `(?#...)`, inline flags `(?imsx)` at
  the start and scoped `(?i:...)` / `(?-i:...)`, `\A` `\Z` `\B`, `\xhh`
  `\uhhhh` `\Uhhhhhhhh` and octal escapes, `{,n}`, and `{` read as a literal
  where it is not a repeat.
- **Flags**: `IGNORECASE`, `MULTILINE`, `DOTALL`, `VERBOSE`, `ASCII`,
  their one-letter names and `NOFLAG`; `LOCALE` raises CPython's
  ValueError for a str pattern.
- **Empty matches**: CPython 3.7's rule throughout `findall`, `finditer`,
  `sub` and `split`: an empty match is not allowed where the previous match
  ended empty, so `re.sub('x*', '-', 'abxd')` is `'-a-b--d-'`.
- **Errors**: `re.error` with CPython's message, `msg`, `pattern` and
  `pos` for unbalanced and unterminated groups, nothing to repeat, multiple
  repeat, bad escapes, bad ranges, unknown and redefined group names,
  invalid and open-group references, global flags not at the start,
  incomplete escapes and variable-width lookbehind.

## Fixed along the way (wrong answers in the old engine)

- `fullmatch` took the first match and then checked its end, so
  `fullmatch('a|ab', 'ab')` was None. The end is now a condition of the
  match, so backtracking finds the alternative.
- `.` matched `\n`; without `DOTALL` it does not.
- `[\D]`, `[\W]` and `[\S]` inside a class were `\d`, `\w` and `\s`; they
  are now the complements.
- `$` matched only at the very end; it also matches before a final
  newline, and `\s` now includes `\x1c`-`\x1f`, as CPython's does for str.
- A quantified group that can match empty (`(a*)*`) looped forever. Each
  loop now stops after an iteration that consumed nothing, which is the
  answer CPython gives.
- `Match.groups()` returned a list; it is a tuple. `group(n)` of a group
  that did not take part returned `''`; it is `None`.

## Not the same

Each is refused where it would otherwise give a different answer:

1. **`findall` with two or more groups** returns tuples, and with fewer
   returns strings. That is one function whose element type depends on the
   group count, which is known only when the pattern is parsed at run time,
   so its results do not type together (measured: the list of `{str,
   tuple}` is "illegal call argument type" at the first use). pyc raises
   NotImplementedError naming `[m.groups() for m in re.finditer(...)]`,
   which gives the same tuples. Typing it needs the group count as a static
   fact: a pattern that is a constant string could be parsed at compile
   time, or `findall` split on the pattern's group count as a demand. This
   is the one open design item.
2. **`Match.lastindex` when no group matched** is None in CPython. `{int,
   None}` has no representation ("mixed basic types"), so that case raises
   NotImplementedError instead of returning a made-up -1. `lastgroup`,
   `{str, None}`, is exact.
3. **Case folding and `\w` `\d` `\s` are ASCII.** pyc's str indexes UTF-8
   bytes ([203](203-str-counts-utf8-bytes.md)), so `.` and a class match
   one byte of a non-ASCII character. A non-ASCII literal matches itself.
4. **Refused with re.error**: possessive quantifiers (`a*+`, 3.11), atomic
   groups `(?>...)`, conditionals `(?(1)...)`. Each message names pyc.
5. **`m.group(1, 2)`** (several arguments) is not supported; `m.groups()`
   is the same tuple.
6. **A capture inside a quantified group** made by an iteration that later
   failed can survive into the match in rare backtracking cases, where
   CPython's would be reset.

## Performance

The engine is Python, compiled. On 20,000 lines with two searches each it
runs in 84 ms against 177 ms for the old engine and 17 ms for CPython's C
engine; `findall`, `sub` and `split` over 40,000 words are 1.3-2.6x
CPython. Matching is recursive at each SPLIT and loop iteration, so a
greedy loop over a very long subject (100k+ characters) uses deep C stack.
The obvious next step is skipping to a pattern's literal first character
in `search`.

**Compile time** is the larger cost, and it is not this module's: it is
[ifa/196](../ifa/issues/closed/196-the-setter-stage-splits-on-a-fact.md)'s SETTER
stage. One test using most of the module took 24 s to compile (54 analysis
passes; 18 s of flow analysis); with `PYC_NOSETTER=1` it takes 12 passes and
3 s and prints the same output. The tail of passes is SETTER splitting
`_ReParser`, `Pattern` and `Match` and every method then splitting on
`self`. `re_search.py` went from 1.6 s to 4.2 s, and in the corpus minilight
from 26 s to 35 s and sunfish from 53 s to 66 s. The measurement is recorded
in ifa/196 as a witness; the test was split into three so the suite stays
inside its 60 s per-test limit. Reshaping this module to dodge the stage was
rejected: the defect is the stage's.

## Tests

- `tests/re_find_sub.py` (findall, finditer, sub/subn with templates and
  functions, split), `tests/re_match_flags.py` (Match and Pattern
  attributes, flags, the old engine's bugs) and `tests/re_syntax.py`
  (backreferences, lookaround, escape, re.error): 41 lines, one test until
  its compile time (above) made it three.
- `tests/re_edge.py`: the empty-match rules, `pos`/`endpos`, anchors, case
  folding, quantifiers, class escapes, templates, and 16 error messages;
  63 lines.
- `tests/re_refusals.py`: the two run-time refusals above
  (`.python.expect_fail`: CPython answers both).
- `tests/re_search.py`, unchanged, now with `groups()` a real tuple.

All identical to CPython on the C and LLVM backends and under `--strict`.

## Follow-ups

- The static group count for `findall` (item 1).
- `textwrap`'s `_split_hyphenated` ([205](205-textwrap.md)) is CPython's
  `wordsep_re` written out by hand because `re` had no lookbehind or
  `findall`; it could now use the regex itself.
