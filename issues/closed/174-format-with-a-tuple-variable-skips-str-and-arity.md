# 174 — `fmt % t` with a tuple VARIABLE: no `__str__`, and no expansion of a runtime-length tuple

**Status:** CLOSED 2026-10-09 (see "Fixed" at the end). Root-caused
2026-10-05. Found as genetic2's last
blocker (its final `print` of the best genome). Reproducible on both
backends. **Silent: compiles clean, exit 0 from pyc, then a segfault or
raw memory in the output.**

**Related:** [168](../168-object-at-a-computed-format-s-prints-garbage.md)
(the same missing `__str__`, for a COMPUTED format),
[165](165-percent-d-truncates-a-64-bit-int-to-32-bits.md) (the
per-argument type tag), ifa/109 (a tuple slice gets list layout).

## Symptom

The format is a constant in every row. Only the right operand varies.

| program | CPython | pyc (C) | pyc (`-b`) |
| --- | --- | --- | --- |
| `"<%s %s>" % (1, 2)` — literal tuple | `<1 2>` | `<1 2>` | `<1 2>` |
| `t = (1, 2); "<%s %s>" % t` | `<1 2>` | segfault | segfault |
| `t = (P(), 7); "<%s %s>" % t` | `<P! 7>` | segfault | segfault |
| `t = tuple([1, 2]); "<%d %d>" % t` | `<1 2>` | `<123838481731488 123838481735584>` | `<124520041365408 16>` |
| `t = tuple([1, 2]); "(%s %s)" % t` | `(1 2)` | raw bytes | empty / crash |
| genetic2: `"(if %s then %s else %s)" % self.args` | the genome | segfault in `_CG_format_string_tagged` | — |

## Root cause

There are two separate gaps, at the two places `%` is handled.

**1. The frontend pre-converts only a LITERAL tuple.**
`python_ifa_build_if1.cc`, `PY_binop` / `PY_OP_MOD`: with a constant
format, it walks the conversions and wraps each `%s` / `%r` argument in
`__str__` / `__repr__`. But it does that only when
`n->children[1]->kind == PY_tuple` (a tuple display with exactly
`convs.n` elements), or for a single non-tuple argument when there is
exactly one conversion. A tuple held in a variable or a field matches
neither, so it goes to `__pyc_format_string__` raw. Codegen then passes
`t->e0, t->e1` straight into `vsnprintf`'s `%s`, which reads an `int64`
or an object pointer as a `char *`.

**2. Codegen expands only a fixed-arity RECORD.**
`format_string_codegen` (`python_ifa_main.cc`) tests
`is_tuple = type_kind == Type_RECORD && !cg_has_classtag(...)`. A tuple
whose length is known only at run time has LIST layout (ifa/109:
`tuple(list)`, and every tuple slice and concatenation of slices). So
it is not a record, and the whole tuple is passed as ONE pointer
argument with ONE tag. genetic2's `TreeNode.args` is built by
`tuple([...])` and recombined by slicing in `crossover`, so it is always
list layout.

## Proposed fix

Do the conversion in the frontend for any tuple-typed right operand, not
only a display. Unpack the operand by index, in the same IR form a
literal tuple's elements take, then apply the existing per-conversion
`__str__` / `__repr__` wrapping and the fresh `make tuple`. For a fixed
arity, the index count is `convs.n`. That is also what CPython requires:
it raises `TypeError: not enough arguments` / `not all arguments
converted` when the counts differ, and a length check at the unpack
reproduces that. This covers gap 2 too, because the operand that reaches
codegen is then always a fresh fixed-arity record.

Whether the right operand IS a tuple is a type question that the
frontend cannot answer: `"%s" % x` with `x` a tuple formats its
elements, and with `x` anything else formats `x`. If the frontend
cannot decide, the decision belongs after FA, where the operand's type
is known. That is the same split 168 faces. Decide this before
implementing, and keep 168 in the same design.

## Verification plan

- Add every row of the table to `tests/string_format.py` (or a new
  `format_tuple_variable.py`), byte-identical to CPython on both backends.
- Also: arity mismatch (`"%s %s" % t` with `len(t) == 3`) raises
  CPython's `TypeError`.
- genetic2: its final line, `Finished in ... best individual: Genome:
  (...)`, matches CPython. All 101 `Epoch:` lines already match.

## Fixed (2026-10-09)

The design question above (is the right operand a tuple?) is answered by
DISPATCH, so it needs no type in the frontend and no post-FA pass.

- **Frontend** (`python_ifa_build_if1.cc`, `PY_OP_MOD` with a constant
  format): any right operand other than a matching tuple display -- a
  tuple variable or field, a single value, or a display of the wrong
  length -- lowers to `x.__pyc_fmtargs__(n)` (n = the conversion count),
  then indexes 0..n-1 and takes the literal display's path: `%s` / `%r`
  converted, a fresh fixed-arity tuple made. That closes both gaps: the
  elements get `__str__`, and what reaches `format_string_codegen` is
  always a record, never a list-layout tuple. Formats with a mapping key
  (`%(k)s`) or a `*` width keep the raw path.
- **`__pyc__`**: `tuple.__pyc_fmtargs__` returns the tuple after
  CPython's two count checks (`not enough arguments for format string`,
  `not all arguments converted during string formatting`);
  `__pyc_any_type__.__pyc_fmtargs__` wraps anything else as `(self,)`.
  On `__pyc_any_type__`, not `object`: int, float and str do not reach
  `object`'s methods. The send is followed by an exception check, so the
  `TypeError` reaches a handler.

genetic2 then matched CPython on the C backend but segfaulted under `-b`:
its `__deepcopy__` receiver is a union of the original's and the copies'
CreationSets, so the copy has no compile-time size. The LLVM backend sized
it from the union's struct (16 of `TreeNode`'s 168 bytes), leaving the
copy's method slots NULL, and the new `__str__` dispatch on a
`{None, TreeNode}` element called address 0. It now calls
`_CG_prim_copy_any` (run-time `GC_size`), as the C backend does
(`cg_emit_llvm.cc`, exported from `pyc_runtime.c`).

Tests: `format_tuple_variable` (every row of the table, the count errors,
single values), `deepcopy_union_receiver_keeps_method_slots` (segfaulted
under `-b` without the copy fix). genetic2 matches CPython on both
backends except its two timing values.

Sweep `047b6a38+4d71b5db`: genetic2 139 -> 0, stdout matches. Contours
rise in most programs (typically +3 ES / +8 CS; more where formatting is
heavy: timsort +35 ES), consistent with one `__pyc_fmtargs__` contour per
receiver type and a fresh argument tuple per format site; not attributed
per program. Its compile wall +68% and three new run timeouts (chaos,
rubik2, tonyjpegdecoder) were machine load: re-measured ALONE, old tree vs
new back to back, compile +2..11% (pystone 1.33 -> 1.48 s), run times and
outputs unchanged (chaos 82 s, tonyjpegdecoder 94 s, rubik2 70 s, rc 0;
chaos's image byte-identical).
