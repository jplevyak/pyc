# 214 — `struct`: every format code; `array`: the float typecodes

**Status:** closed 2026-11-25 on landing, filed and done in one change.
What is left: `struct.Struct`, `pack_into` and `iter_unpack`; a format
built at run time takes the integer codes only; and the rest of `array`'s
methods (`count`, `index`, `insert`, `pop`, `remove`, `reverse`,
`byteswap`, `+`, `*`, `==`).

## Symptom

`struct` packed and unpacked the integer codes only: `e f d ? c p` raised
`ValueError`, `s` worked in `unpack` alone, and native mode (`@`, or no
prefix) neither aligned fields nor used native sizes (`calcsize('bi')` was
5, CPython 8; `'l'` was 4 bytes, CPython 8 on LP64). No `struct.error`
existed, and nothing range-checked a value. The constant-format lowering
reached only `unpack(...)` after `from struct import unpack`, never
`struct.unpack(...)`. `array` had no float typecodes, and its `'l'` was 4
bytes.

## struct

`pyc_lib/struct.py` takes every code (`x c b B ? h H i I l L q Q n N P e f
d s p`), every byte order, native sizes and alignment, and CPython's
`struct.error` / `OverflowError` messages for range, type, count, buffer
size and bad formats.

**Typed fields need the format at compile time.** A record of an int, a
float and a bytes is a tuple with a type per position; a loop that builds
one list cannot produce it. So a call whose format is a constant string is
lowered by the frontend to one call per field
(python_ifa_build_if1.cc, `lower_struct_call`, the existing unpack
lowering extended to `pack` and to every code):

```python
pack('<If', n, x)        -> _cat(_pack_int(n, "I", 4, False, True), _pack_float(x, 4, True))
unpack_from('<If', d, o) -> b = _unpack_base(d, o, 8, True)
                            (_unpack_int(d, b, 0, 4, False, True), _unpack_float(d, b, 4, 4, True))
```

It matches the callee by symbol for both `pack(...)` and `struct.pack(...)`,
and emits the pending-exception check after each call. Without that check a
raise inside `_pack_int` stayed pending and the call returned `b''`. A
format it cannot read (not a literal, an escape, a bad code or count) is
left to the loop versions, which raise CPython's `struct.error`. Those loop
versions are what a run-time format reaches; they take the integer codes,
and any other code raises `NotImplementedError` naming the constant form.

IEEE bits come from six runtime helpers (`_CG_float_bits64/32/16` and
their inverses, pyc_c_runtime.h). C's double-to-float and
double-to-`_Float16` conversions round to nearest-even, as CPython's
`PyFloat_Pack4` / `PyFloat_Pack2` do; an infinity from a finite value is
CPython's `OverflowError`.

## array

`array('f', ...)` and `array('d', ...)` with a constant typecode build
`_floatarray`, a subclass of `array` whose list holds floats
(python_ifa_build_if1.cc, `float_array_class`, by the class's symbol). The
element type is part of the class, and the typecode is the only thing that
says which. `'f'` rounds each value to binary32 on the way in, so a read
returns what `tobytes` writes, as in CPython. A float typecode that is not
a constant reaches `array` and raises `NotImplementedError`. Also added:
`itemsize`, `repr`/`str`, `in`, `'l'`/`'L'` at 8 bytes, CPython's
bad-typecode `ValueError`, and `must be real number` for a non-number. That
message names the type only for the builtin types, since pyc has no
`type(x).__name__` ([190](190-class-name-attribute.md)).

## Found on the way: `isinstance(True, int)` was False

`_pack_int`'s `if not isinstance(v, int): raise ...` raised on every bool,
so `pack('<f', True)` raised, and so would CPython code that checks an
argument the same way. ifa made `bool` a specializer of `anyint` under the
`bool_is_numeric` opt-in (ifa/081) but never an instance of the language's
`int` (`int64`). Fixed in ifa/if1/ast.cc under the same opt-in: `bool` now
implements `int64`. `implementors` is the is-instance relation that
`isinstance`, `issubclass`, their narrowing and codegen's tag test read.
Dispatch reads `specializers`, which is unchanged, so `bool` keeps its own
methods.

Narrowing now sends a bool down a program's int branch, and a bool
receiver had no `>> << // % ** /`, unary `-` or `~`, or `bit_count` at all
(each was a compile error, `unresolved call '__rshift__'`). Those are added
to `class bool` (`__pyc__/00_runtime.py`), each on the value 0 or 1 in the
form `__and__` already uses. `int(True)` is avoided there, as its comment
records for the LLVM backend.

A second thing seen while reducing this, not fixed: when an always-raising
call leaves the rest of the module unreachable, the analysis of that dead
code can report type errors (`illegal primitive argument type` on a list
that holds one type in any live run). Here it showed up as a merged
`codes`/`counts` pair in `struct._codes`, which disappeared once the raise
was caught or no longer certain.

## Verification

- `tests/struct_all_formats.py`: every code, both orders, native alignment
  and sizes, mixed records, the member form, and 20 error cases.
- `tests/array_float_typecodes.py`: `f` and `d` through both import forms,
  rounding, bytes both ways, slicing, `del`, `in`, the 8-byte `l`/`q`, and
  the errors.
- `tests/bool_is_int.py`: `isinstance`, `issubclass`, narrowing, and every
  new bool operator.

All three are identical to CPython on the C and LLVM backends and under
`--strict`. The test needed separate int and float lists: a list literal
mixing them, or `sum()` of floats, is widened (refused under `--strict`)
in any program ([215](215-sum-of-floats-refused-under-strict.md)).

`make test`: every gate passes, 607 passed and 0 failed on both backends.

Corpus: `check__default__5499f7f7+7e701eac` has every verdict of
`check__default__6b970e6b+c656d7e3` (the sweep of the tree before):
compile, warnings, run and stdout unchanged for all 77, with othello2 the
one stdout difference in both. Contours moved only in programs that import
struct or array, or compare bools:

| program | ES | CS | |
| --- | --- | --- | --- |
| minpng | 259 → 245 | 1093 → 1039 | struct.pack lowered |
| sha | 355 → 345 | 1299 → 1275 | struct.pack lowered |
| doom | 808 → 815 | 2291 → 2404 | unpack_from |
| msp_ss | 1049 → 1258 | 8646 → 9405 | see below |
| mao, plcfrs, pygasus, rdb, tarsalzp | +3 to +6 | +29 to +53 | array |
| genetic2, quameon, mastermind2 | -1, -2, +15 | -1, -2, +13 | |

**msp_ss, measured.** With only the bool change applied to HEAD it stays at
1049 EntrySets, so the growth is the struct lowering. Every added contour
has the same types as an existing one: `int.__eq__(int64, int64)` goes from
61 contours to 89, and msp_ss's own `comTxRx` from 1 to 7. They are
constant splits ([ifa/151](../ifa/issues/151-split-an-entryset-on-a-constant-argument-on-demand.md)'s
CONST_DEMAND). Both builds enter it with the same violations (63 at the
first pass it runs, then 8, then 0). In the new build its backward walk
passes through the lowered readers' constant `size` formals
(`_uint_bytes`, `_unpack_int`) and up into `bslTxRx(cmd)` and
`comTxRx(cmd)`, which then split on their constant `cmd`; 51 formals are
nominated against 34. FA went from 12.2 s to 13.9 s. Whether a walk that
reaches a caller's unrelated constant is earning its keep is a question
about 151's walk, not about struct, and is left there.
