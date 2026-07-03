# Issue 006: f-strings compile silently but never interpolate

**Status:** partially fixed. Interpolation (`{expr}`, `{{`/`}}`
literal braces, `!s`/`!r`/`!a` conversions, nesting/indexing/method
calls inside the field) works on both backends. Format specs
(`{x:spec}`) are intentionally a hard compile error for now rather
than a silent no-op — see "Remaining gap" below.
**Affects:** `python_ifa_build_if1.cc` (`build_fstring_pyda`,
`scan_fstring_field`, `build_fstring_subexpr_pyda`,
`fstring_append_piece`, `decode_string_content`,
`skip_string_prefix` — all new; `eval_string_pyda` now delegates
prefix/quote parsing to the shared `skip_string_prefix`/
`decode_string_content` helpers); `python_parse.{h,cc}`
(`dparse_python_buf_to_ast` — new in-memory-buffer parse entry
point, factored out of `dparse_python_to_ast`);
`python_ifa_build_syms.cc`/`python_ifa_int.h` (`build_syms_pyda`
un-`static`'d so the f-string code can run it over synthesized
sub-expression ASTs).
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

## What landed

Implemented in `python_ifa_build_if1.cc`'s `PY_string` case:

1. **`dparse_python_buf_to_ast`** (`python_parse.{h,cc}`): parses
   an in-memory buffer rather than a file, factored out of
   `dparse_python_to_ast`'s shared `dparse_buf_to_ast_impl`. Used
   to re-parse each `{expr}` field's raw source (wrapped in
   parens: `"(" + expr + ")\n"`) as a standalone expression —
   confirmed empirically via `--dparse_ast` that this produces
   `module -> expr_stmt -> <expr>`, so the expression node is
   `mod->children[0]->children[0]`.
2. **`build_syms_pyda` exposed** (was `static` in
   `python_ifa_build_syms.cc`): the f-string code calls it (then
   `build_if1_pyda`) directly on the freshly-parsed sub-expression
   AST, at whatever scope (`ctx.scope_stack`) is active at the
   point the f-string literal appears — so `{x}` resolves `x` in
   the enclosing function/module scope like any other reference,
   not a fresh module scope.
3. **`scan_fstring_field`**: splits one `{...}` field into
   expr-text / optional `!s`\|`!r`\|`!a` conversion / optional
   `:spec` format spec. Tracks paren/bracket/brace nesting depth
   (so `f"{d[1:2]}"`-style content and dict/set literals inside
   the field don't get mis-split) and skips over quoted
   substrings (so `f"{d['a:b']}"` works) — conversion/format-spec
   markers are only recognized at nesting depth 0. Does **not**
   handle triple-quoted strings nested inside a field (rare;
   accepted as a known gap since a mis-scan there surfaces as a
   parse error on the extracted sub-expression, not silent wrong
   output).
4. **`build_fstring_pyda`**: walks the string content, collapses
   `{{`/`}}` to literal braces, decodes each literal run through
   the (now-shared) `decode_string_content` helper, lowers each
   field's expression, stringifies it (`__str__` for no conversion
   or `!s`; `__repr__` for `!r`/`!a`) via `call_method`, and
   concatenates every piece left-to-right with `__add__` (the same
   dispatch `str.__add__`/`PY_binop`'s `+` already uses).
5. **Format specs are a hard `fail()`**, not a silent no-op — the
   whole point of this issue is that pyc must never again silently
   produce the wrong string. `f"{x:.2f}"` fails cleanly at compile
   time with a message naming the unsupported spec, rather than
   compiling and dropping the `.2f`.

Bug caught during manual testing (not by the automated suite,
since none existed yet): the closing-`}` handler in
`scan_fstring_field` initially overwrote `expr_end` even when a
`!conv` had already set it, so `f"{s!r}"` mis-extracted `s!r` as
the expression text (itself then failing at re-parse). Fixed by
guarding the closing-brace assignment with `if (!expr_end)`.

Verified (`tests/fstring_basic.py` + `.exec.check`, output diffed
byte-for-byte against real `python3`): basic interpolation,
`{{`/`}}` escapes, arithmetic in the field, multiple fields in one
literal, empty/no-interpolation f-strings, `!r`/`!s` conversions,
dict-index and method-call expressions inside a field, and
triple-quoted f-strings. Passes on both the C and v2 LLVM backends
(110/0 on `./test_pyc` and `PYC_FLAGS="-b" ./test_pyc`, up from
109/0 before this fix — no regressions).

## Remaining gap

**Format specs** (`{x:.2f}`, `{x:>10}`, `{x:,}`, etc.) are not
implemented — `scan_fstring_field` already extracts the spec
string cleanly, so wiring it up means mapping PEP 3101's mini
format-spec language onto pyc's existing `_CG_*`
number-formatting primitives (the same ones `"%f" % x`-style
`__pyc_format_string__` uses; see `01_str.py`'s `__mod__`). Worth
its own follow-up issue once someone hits a real need for it;
today it's a clean compile error rather than silently dropped.

## Verification plan (for the format-spec follow-up)

1. Width/precision: `f"{3.14159:.2f}"` → `"3.14"`.
2. Alignment/fill: `f"{'x':>5}"` → `"    x"`.
3. Integer presentation types: `f"{255:x}"` → `"ff"`.
4. Existing `tests/fstring_basic.py` continues to pass unchanged.

## What this unblocks

f-strings are the standard modern-Python string-formatting idiom;
before this fix, any ported CPython code using them miscompiled
silently rather than failing loudly — a trap for anyone porting
code to pyc without independently noticing the wrong output. Basic
interpolation (the overwhelming majority of real-world f-string
usage) now works correctly on both backends.
