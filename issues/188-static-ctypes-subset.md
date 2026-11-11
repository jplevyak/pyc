# 188 — a static subset of `ctypes`: calling C libraries from pyc programs

**Status:** open. The core landed 2026-11-11 (`python_ifa_ctypes.cc`,
`pyc_lib/ctypes/`, tests `ctypes_*`); what is left is the list under
[Not done](#not-done).

## What it is

A program that declares its foreign functions the way CPython's `ctypes`
asks has stated the library, the symbol and its C signature, which is
everything a static compiler needs:

```python
import ctypes
glib = ctypes.CDLL("libglib-2.0.so.0")

class GString(ctypes.Structure):
    _fields_ = [("str", ctypes.c_char_p), ("len", ctypes.c_size_t),
                ("allocated_len", ctypes.c_size_t)]

glib.g_string_new.argtypes = [ctypes.c_char_p]
glib.g_string_new.restype = ctypes.POINTER(GString)
s = glib.g_string_new(b"hello")
print(s.contents.str, s.contents.len)
```

pyc reads those declarations at compile time and emits a direct call and a
link entry. The same file runs under CPython, so the suite and the sweep
compare against it as usual. It is CPython's surface, not a pyc dialect:
`__pyc_c_call__` stays the internal primitive.

## How it works

`lower_ctypes` (python_ifa_ctypes.cc) runs on every module's AST before
`build_syms`, and rewrites the declarations into ordinary pyc Python plus a
C shim. Nothing in ifa knows about it.

| source | becomes |
| --- | --- |
| `lib = CDLL("x")` | an instance of a generated class with one METHOD per declared function, and a link entry (`-l:x`; a path is passed as written; `find_library("m")` is `-lm`; `None` links nothing) |
| `lib.f.argtypes = [...]`, `lib.f.restype = T` | compile-time only; the statement is erased |
| `lib.f(a, b)`, `g = lib.f` | untouched: plain method dispatch and a plain bound method, from any module |
| `class S(Structure)` with `_fields_` | a class over C-layout storage, a getter and a setter per field |
| `POINTER(S)` | a generated pointer class per `S`: nominal, so `POINTER(S)` and `POINTER(T)` do not mix |
| `byref(s)`, `pointer(s)` | run-time functions in `pyc_lib/ctypes` |

Three decisions carry the design:

- **Argument checking is method dispatch.** Each wrapper converts its
  arguments with a method only the acceptable types define
  (`a.__pyc_ffi_int__()` exists on `int` and `bool`, not `float`), so a
  wrong argument is an ordinary compile-time type error where CPython
  raises `ctypes.ArgumentError`. Those methods are added to the builtin
  classes only for a program that imports `ctypes`.
- **The C compiler owns the ABI.** The shim defines `_CG_ffi_<lib>_<f>`
  over machine words and doubles only and narrows to the declared C types
  itself (`int`, `float`, `unsigned short`, a struct by value). Both
  backends call it the same way; the C backend gets it in the generated
  file, the LLVM backend compiles it as a second translation unit
  (`codegen_extra_source`). The foreign symbol is declared under a private
  name with an asm label, so it cannot clash with a header's prototype.
- **A pointer is an object, as in CPython.** A `POINTER(S)` result is an
  instance holding the address. NULL is a falsy instance, not `None`, which
  is CPython's behaviour and needs no nullable-scalar representation.

A Structure's storage is a collected, scanned buffer, so a `bytes` stored in
a `c_char_p` field stays alive as long as the struct does. `p.contents` and
`p[i]` are views that alias the pointee.

Field stores needed one new frontend mechanism, the store-side twin of the
`@property` read: `o.NAME = v` for a registered name is
`o.__pyc_set_NAME__(v)`, with a default on every builtin class that stores
the field (`emit_member_store`, `pyc_is_setter_name`). `tests/ctypes_struct`
has `x`, `name` and `tag` as struct fields and as plain fields of ordinary
classes in one program. `@NAME.setter` is still refused (issues/007); this
is the dispatch it was waiting for.

## Flags

`--cflags` / `PYC_CFLAGS` and `--libs` / `PYC_LIBS` pass extra compile and
link flags to the generated program on both backends, for a library outside
the default search path (`--libs "-L. -lfoo -Wl,-rpath,."`).

## Deviations

| | |
| --- | --- |
| a `c_void_p` result or field | CPython yields `int`, or `None` for NULL. `PYC_FFI_VOIDP_INT` (permissive, warns at the declaration) yields the address with NULL as 0; `--strict` refuses. `POINTER(S)` avoids it. |
| an unsigned 64-bit result above 2^63 | wraps negative: pyc's `int` is 64-bit. Not flagged. |
| a `None` passed where a C integer is declared | a run-time `AttributeError`, where CPython raises `ArgumentError`; both at run time. |

## Refused, with the line

A library that is not a string literal, `None` or `find_library("lit")`; a
function called with no `argtypes` (pyc does not guess a signature from the
arguments); a declaration that is not at module top level; and any other use
of a ctypes name (`c_int(5)`, `cast`, `sizeof`, `CFUNCTYPE`).

## Not done

1. **Callbacks** (`CFUNCTYPE`). A pyc function is cloned per contour and C
   needs one entry point, so the declared signature has to seed flow
   analysis as an external caller. Anything a library retains
   (`user_data`) then needs pinning, because Boehm does not scan the
   library's heap.
2. **Scalar boxes and out-parameters**: `x = c_int(); f(byref(x)); x.value`,
   and `POINTER(c_int)`. The `GError **` idiom needs this.
3. **Arrays** (`c_int * 4`) and a Structure held by value in a field.
4. **`Union`, bit fields, `_pack_`.**
5. **Declarations across modules**: `POINTER(S)` must be written in the
   module that defines `S`. Generated bindings do this anyway.
6. **`--jit`** refuses a program with foreign calls: the shim and the
   library are linked, not loaded.
7. **Generating the declarations** from headers (libclang) or from
   GObject-Introspection `.gir` files, which also carry the ownership
   annotations item 1 needs. A separate tool whose output is this subset.
8. **macOS** is written (`-lNAME` from `libNAME.dylib`,
   `__USER_LABEL_PREFIX__`) and untested.

## Verification

`tests/ctypes_scalars`, `ctypes_struct`, `ctypes_bindings` pass on both
backends and are byte-identical to CPython, also under `--strict`. Four
`ctypes_*_refused` tests pin the diagnostics. glib itself (`GString`,
`GList`) was run by hand against `libglib-2.0.so.0`; it is not in the suite
because CI may not have the library.
