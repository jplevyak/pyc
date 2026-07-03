# Issue 006: f-strings compile silently but never interpolate

**Status:** open.
**Affects:** `python_ifa_build_if1.cc:153` (`eval_string_pyda`).
**Related:** none.

## Symptom

An f-string compiles with **no error or warning** and produces
the literal source text (prefix, quotes, braces and all) instead
of the interpolated value.

```python
x = 5
print(f"value is {x}")
```

Generated C (`fstring_test.py.c`):

```c
_CG_write(_CG_String("f\"value is {x}\""));
```

Running the compiled binary prints:

```
f"value is {x}"
```

instead of the correct:

```
value is 5
```

This is a **silent correctness bug**, not a rejected-input error
— worse than a compile failure because the program appears to
compile and run successfully.

## Root cause

`python.g`'s `stringprefix` rule (~line 417-419) already accepts
`f`, `F`, `fr`, `fR`, `Fr`, `FR`, `rf`, `rF`, `Rf`, `RF` — so the
grammar happily parses f-string syntax as an ordinary `STRING`
token, braces and all, with no distinction from a plain string.

`eval_string_pyda` (`python_ifa_build_if1.cc:153`) is the
hand-written literal-string evaluator introduced when CPython's
`PyRun_String`-based parser was removed (see project memory,
"Phase 6: Full CPython Cutover"). Its prefix-skip loop only
recognizes `r/R/b/B/u/U`:

```cpp
while (*s && (*s == 'r' || *s == 'R' || *s == 'b' || *s == 'B' || *s == 'u' || *s == 'U')) {
  if (*s == 'r' || *s == 'R') is_raw = true;
  s++;
}
```

For an `f"..."` literal, `s` still points at `f` after this loop,
so the subsequent `char q = *s;` sees `'f'`, which isn't a quote
character, and the function bails out via
`if (q != '\'' && q != '"') return make_string(s);` — returning
the **entire original token, prefix and all**, as a plain string
constant. No parsing of `{expr}` segments ever happens; the
grammar doesn't even distinguish an f-string's braces from any
other character in the literal.

## Proposed fix sketch

1. **Frontend split**: when `eval_string_pyda` detects an `f`/`F`
   prefix, don't treat the token as one opaque string. Split the
   (already-parsed, escape-decoded) content on `{` / `}` pairs
   (handling `{{`/`}}` as literal braces per PEP 498), and for
   each `{expr}` segment, parse `expr` as a Python expression
   (reusing the existing DParser entry point for standalone
   expressions, or extending `python.g` to capture the sub-source
   span so a nested parse can run over it) and lower it via
   `__pyc_to_str__` (already listed as an existing magic method
   hook in `PYTHON_FRONTEND.md` §11, "Language extensions" —
   apparently wired for `str(...)` but never connected to
   f-strings).
2. Concatenate the literal segments and stringified expression
   results with the existing `__pyc_format_string__` / string
   concat machinery.
3. Grammar-level alternative: give f-strings their own token kind
   distinct from plain `STRING` so the frontend can special-case
   them without re-deriving "was this an f-string" from the
   already-stripped prefix.

## Verification plan

1. `print(f"value is {x}")` with `x = 5` prints `value is 5`.
2. Escaped braces: `print(f"{{literal}}")` prints `{literal}`.
3. Nested expressions: `print(f"{x + 1}")`, `print(f"{obj.method()}")`.
4. Format-spec suffix (`f"{x:.2f}"`) at least doesn't regress
   worse than today (may be filed as its own follow-on if full
   format-spec support is out of scope for the first pass).
5. Add `tests/fstring_basic.py` + `.exec.check` — first f-string
   test in the suite (none exist today; confirmed via
   `grep -l 'f"' tests/*.py` returning no real hits, only
   coincidental substring matches like `"asdf"`).

## What this unblocks

f-strings are the standard modern-Python string-formatting idiom;
any ported CPython code using them currently miscompiles silently
rather than failing loudly, which is a trap for anyone porting
code to pyc without independently noticing the wrong output.
