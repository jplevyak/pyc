# Issue 006: f-strings compile silently but never interpolate

**Status:** fixed. Interpolation (`{expr}`, `{{`/`}}` literal braces,
`!s`/`!r`/`!a` conversions, nesting/indexing/method calls inside the
field) and format specs (`{x:spec}`, PEP 3101 mini-language) both
work on both backends. See "Format specs: what landed" below.
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

## Format specs: what landed

Implemented the PEP 3101 format-spec mini-language
(`[[fill]align][sign]["#"]["0"][width][","|"_"]["." precision][type]`)
via a `__format__` dunder method, dispatched exactly like `__str__`/
`__repr__` (mirrors CPython's `format(x, spec)` → `type(x).__format__`):

1. **`pyc_c_runtime.h`/`pyc_runtime.c`.** Added `_CG_parse_format_spec`
   (parses the spec string into fill/align/sign/alt/zero/width/
   group/precision/type), `_CG_group_digits` (comma/underscore
   thousands grouping), and `_CG_pad_align` (width/fill/alignment,
   including the cases plain `printf` can't do at all — a custom
   fill character, center alignment, or `=` padding-after-sign) as
   shared helpers, plus three type-specific formatters built on top:
   `_CG_format_int_spec`, `_CG_format_float_spec`,
   `_CG_format_str_spec`. Numeric conversions (`f`/`F`/`e`/`E`/`g`/`G`/
   `%`/`d`/`x`/`X`/`o`/`b`) reuse ordinary `printf` conversions for
   the "core" digit string wherever printf already does the right
   thing; grouping and alignment/fill are applied afterward,
   uniformly across all three types. Exposed to the LLVM backend via
   the same `inline`-in-header-plus-`extern`-in-`pyc_runtime.c`
   pattern as `_CG_str_from_int`/`_CG_str_from_float`.
2. Added `__format__(self, spec)` to `int`/`float`/`str`
   (`__pyc__/02_numeric.py`, `01_str.py`), each a thin
   `__pyc_c_call__` wrapper around the matching `_CG_format_*_spec`.
   `bool.__format__` (`00_runtime.py`) special-cases an empty spec to
   `self.__str__()` (`"True"`/`"False"`) and otherwise converts to
   0/1 and delegates to `_CG_format_int_spec`, matching CPython's
   "bool is an int subtype" formatting behavior.
   `__pyc_any_type__.__format__` (the default for classes with no
   override) falls back to `self.__str__()` unconditionally — CPython's
   `object.__format__` raises `TypeError` for a non-empty spec, but
   pyc has no exception model yet (issue 011), so this is
   deliberately permissive rather than matching that error case.
3. `build_fstring_pyda` (`python_ifa_build_if1.cc`) now dispatches to
   `__format__` for any non-empty spec instead of failing to compile.
   If a conversion (`!r`/`!s`/`!a`) was also given, the *converted
   string* is formatted (`str.__format__`), not the original value —
   matches CPython's order of operations (convert, then format).

**Found and fixed along the way**: verifying `f"{1234567.89:,.2f}"`
surfaced a real, pre-existing, unrelated bug — the C backend (not
LLVM) silently corrupted *any* float literal needing more than 6
significant digits (e.g. `1234567.89` became `1234570.0`) when
embedding it as a compiled constant. Root cause: `ifa/if1/num.cc`'s
`sprint_float_val` (used by `cg.cc`'s `write_c` to serialize a float
constant's literal text into the generated `.c` file) used bare
`"%g"` (6 significant digits) instead of `"%.17g"`, unlike
`pyc_c_runtime.h`'s `_CG_str_from_float`/
`_CG_prim_primitive_to_string(double)`, which already use `%.17g`
for exactly this reason. Fixed by matching that existing convention.
This is a codegen-constant-embedding bug, not new corruption from
this change — `sprint_float_val` is core `ifa` code, not part of
this issue's `__format__` work, but the fix was small, obviously
correct (matches an established pattern used two other places
already), and directly needed to verify format specs against
realistic values. Verified `./ifa --test` (58/0) and `make test`
(same 6 pre-existing, unrelated `patterns`-phase failures) unaffected.

Verified against CPython on both backends: width, precision, all
sign variants, `#`/`0` flags, all three alignments plus a custom
fill character, comma grouping (int and float together), percentage
type, all integer presentation types (`d`/`x`/`X`/`o`/`b`/`#x`),
string truncation via precision, `bool`, and a custom class's
`__str__` used inside another f-string. Added
`tests/fstring_format_spec.py` and (for the `sprint_float_val` fix)
`tests/float_literal_precision.py`. Full `./test_pyc` +
`PYC_FLAGS="-b" ./test_pyc`: 121/0 both backends, no regressions.

## What this unblocks

f-strings are the standard modern-Python string-formatting idiom;
before this fix, any ported CPython code using them miscompiled
silently rather than failing loudly — a trap for anyone porting
code to pyc without independently noticing the wrong output.
Interpolation and format specs — the two pieces of PEP 498 — both
now work correctly on both backends. The `sprint_float_val` fix
also removes a silent numeric-precision bug affecting any compiled
float literal, independent of f-strings.
