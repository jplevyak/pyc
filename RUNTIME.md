# RUNTIME — The pyc Runtime Layer

A working reference for the C runtime and Python-side builtin module
that pyc-compiled programs link against. This layer is **not** part
of IFA — it lives in the pyc repository root and is consumed by
the C backend's emitted code plus the frontend's symbol environment.

Sister docs: [PYTHON_FRONTEND.md](PYTHON_FRONTEND.md) (the frontend
that uses these), [ifa/CODEGEN_C.md](ifa/CODEGEN_C.md) (the backend
that emits `_CG_*` calls), [PIPELINE.md](PIPELINE.md).

---

## 1. In one paragraph

A pyc-compiled program has two runtime dependencies. The **C runtime**
(`pyc_c_runtime.h`, 413 lines, single-header) provides type
definitions (`_CG_int32`, `_CG_string`, `_CG_list`, …), allocation
primitives (`_CG_prim_tuple`, `_CG_prim_list`, `_CG_prim_closure`),
operators (`_CG_add`, `_CG_period`, …), I/O (`_CG_write`,
`_CG_writeln`), Boehm GC integration (`MEM_INIT`, `MALLOC`,
`REALLOC`), and string layout helpers. The **Python builtin module**
(`__pyc__/*.py`, 7-8 ordered files) defines Python-level classes
(`object`, `str`, `int`, `float`, `list`, `tuple`, `dict`,
`bytearray`, `range`, `slice`), iterators, and the built-in functions
(`abs`, `all`, `len`, `chr`, `print`, `isinstance`, …) that user
programs can reference. The shim `pyc_compat.py` lets user programs
`import` pyc-specific names without breaking under CPython.

---

## 2. File map

```
pyc/
├── pyc_c_runtime.h          (413 lines) Self-contained C header, all macros / inlines / typedefs.
├── pyc_compat.py            One line: __pyc_declare__ = None.
├── pyc_symbols.h            Macro list S/P/B of frontend-known names.
├── __pyc__/                 Python builtin module (loaded as one concatenated module).
│   ├── 00_runtime.py        __pyc_any_type__, object, __pyc_None_type__, bool, __base_iter__
│   ├── 01_str.py            str
│   ├── 02_numeric.py        int, float
│   ├── 03_slice.py          __slice_iter__, slice
│   ├── 04_sequence.py       __list_iter__, list, __tuple_iter__, tuple
│   ├── 05_builtins.py       abs, all, any, bin, exit, range, len, chr, ord, hex, isinstance, issubclass
│   ├── 06_bytearray.py      bytearray
│   └── 07_dict.py           dict
└── __pyc__.py               Fallback single-file version (used if __pyc__/ doesn't exist).
```

---

## 3. The C runtime header (`pyc_c_runtime.h`)

A single header included by every emitted `.c` file via
`#include "pyc_c_runtime.h"` (the include is emitted by pyc's
`PycCompiler::c_codegen_pre_file` — see
[PYTHON_FRONTEND.md](PYTHON_FRONTEND.md) §8).

### 3.1 Standard headers & GC

```c
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include "gc.h"
#define MALLOC GC_MALLOC
#define REALLOC GC_REALLOC
#define FREE(_x)
#define MEM_INIT() GC_INIT()
```

GC is Boehm; `MEM_INIT()` is the call from emitted `main()`.
`FREE` is a no-op because GC tracks everything.

### 3.2 Type definitions

```c
typedef char           int8;       typedef unsigned char  uint8;
typedef short          int16;      typedef unsigned short uint16;
typedef int            int32;      typedef unsigned int   uint32;
typedef long long      int64;      typedef unsigned long long uint64;
typedef float          float32;    typedef double         float64;
typedef struct { float32 r, i; }  complex32;
typedef struct { float64 r, i; }  complex64;
```

Plus a `_CG_*` alias for each:

```c
typedef int8  _CG_int8;     typedef uint8 _CG_uint8;
typedef int16 _CG_int16;    typedef uint16 _CG_uint16;
... (matching pairs)
typedef char *_CG_string;
typedef void *_CG_symbol;
typedef void *_CG_function;
typedef void *_CG_tuple;
typedef void *_CG_list;
typedef void *_CG_vector;
typedef void *_CG_continuation;
typedef void *_CG_any;
typedef void *_CG_null;
typedef void *_CG_void;
typedef void *_CG_object;
typedef void *_CG_ref;
typedef void *_CG_fun;
typedef void *_CG_nil_type;
typedef uint8 _CG_bool;
```

The `_CG_*` prefix means "Codegen". Every type the C backend emits
uses one of these. The mapping between IF1 Syms and `_CG_*` names is
set up by `cg.cc:build_type_strings` (see
[CODEGEN_C.md](CODEGEN_C.md) §3.2) — `Sym::cg_string` holds the
chosen `_CG_*` name.

### 3.3 Constant helpers

```c
#define _CG_Symbol(_x, _y) ((void *)(uintptr_t)_x)
#define null  ((void *)0)
#define bool  int
#define True  1
#define False 0
#define __init ((void *)0)
#define nil_type 0
```

Symbols at runtime are encoded as their integer Sym id cast to a
pointer. The `_y` second arg of `_CG_Symbol` is the name string,
discarded in the macro but kept for human-readable C output.

### 3.4 String layout

```c
/*
  Strings are pointers to the data portion (C-like) preceeded by
  an 8-byte length. This makes them compatible with C and still
  permits them to contain \0 and makes obtaining the length O(1).

  Strings have a 0 sentinal at the end for C compatibility.
*/
#define _CG_string_len(_s) ((_s) ? (size_t) * (int64 *)(((char *)(_s)) - 8) : 0)
```

So a `_CG_string` points to the first character; the 8 bytes
*before* hold the length. `_CG_String(literal)` allocates a buffer
big enough for the length+data+sentinel and returns a pointer to the
data (past the length).

This layout means:
- `strcmp` / `printf("%s", s)` work directly.
- `len(s)` is O(1) — one pointer subtraction.
- Embedded NULs are fine.

### 3.5 Allocation primitives

The emitted code calls these for object construction:

```c
_CG_prim_tuple(T, n)         // allocate a tuple of n elements typed T
_CG_prim_tuple_list(T, n)    // tuple stored as a list-like
_CG_prim_list(elem, n)       // allocate a list with element type elem
_CG_prim_closure(T)          // allocate a closure of type T
```

Each returns a fresh pointer of the matching `_CG_*` type. The
caller then assigns `result->e0 = arg0; result->e1 = arg1; ...`.

The list/tuple structs are defined per-type by `cg.cc` (see
[CODEGEN_C.md](CODEGEN_C.md) §3.2):

```c
typedef struct Tn *Tn;
struct Tn { T0 e0; T1 e1; T2 e2; ... };
```

So `e0, e1, e2, ...` are the fields. Cloning's
`compute_member_types` (see [CLONE.md](CLONE.md) §8.2) sets up the
`has` for each `Type_RECORD`, which `cg.cc` then translates into
these field declarations.

### 3.6 Numeric operators

Each table primitive (see [PRIMITIVES.md](PRIMITIVES.md) §9) has a
corresponding `_CG_<name>` macro or inline function:

```c
#define _CG_prim_add(a, b)       ((a) + (b))
#define _CG_prim_subtract(a, b)  ((a) - (b))
#define _CG_prim_mult(a, b)      ((a) * (b))
... etc ...
#define _CG_prim_equal(a, b)     ((a) == (b))
#define _CG_prim_less(a, b)      ((a) <  (b))
```

These are macros (not functions) so C's type promotion happens
naturally and the C compiler can constant-fold.

### 3.7 I/O

```c
static inline void _CG_write(const char *s) {
  fputs(s, stdout);
}
static inline void _CG_writeln(void) {
  putchar('\n');
}
```

Used by the registered primitives `write` / `writeln` (see
[PYTHON_FRONTEND.md](PYTHON_FRONTEND.md) §10). The Python `print()`
lowers to `__pyc_to_str__` + `_CG_write` + `_CG_writeln`.

### 3.8 String formatting

```c
_CG_format_string(fmt, val)   // f-string body; pyc's frontend lowers __pyc_format_string__ to this
_CG_String(literal)           // wrap a literal in the length-prefix layout
```

### 3.9 Other helpers

The full set of macros/inlines includes math (`pow`, `sin`, `cos`,
…), bit ops (`_CG_prim_and`, `_CG_prim_or`, `_CG_prim_xor`,
`_CG_prim_lsh`, `_CG_prim_rsh`, `_CG_prim_not`), comparisons
(`==`, `!=`, `<`, `<=`, `>`, `>=`), and the unary forms
(`_CG_prim_plus`, `_CG_prim_minus`, `_CG_prim_lnot`).

Anything you can do in C, the runtime can emit. New primitives need
a matching `_CG_*` macro added here.

---

## 4. The Python builtin module (`__pyc__/`)

Loaded by `pyc.cc:main` at startup, *before* user files. The
directory is found at `$IFA_SYSTEM_DIRECTORY/__pyc__/` or
`$PYC_SYSTEM_DIRECTORY/__pyc__/`. If the directory exists,
`dparse_builtin_dir` (see
[PYTHON_FRONTEND.md](PYTHON_FRONTEND.md) §7) concatenates the sorted
`.py` files into one module and parses them together. If the
directory is missing, `__pyc__.py` (single-file fallback) is used.

The directory layout (8 files):

```
00_runtime.py    __pyc_any_type__, object, __pyc_None_type__, bool, __base_iter__
01_str.py        str
02_numeric.py    int, float
03_slice.py      __slice_iter__, slice
04_sequence.py   __list_iter__, list, __tuple_iter__, tuple
05_builtins.py   abs, all, any, bin, exit, range, len, chr, ord, hex, isinstance, issubclass
06_bytearray.py  bytearray
07_dict.py       dict
```

The numeric prefixes are ordering guarantees: forward references
work *within* the concatenated module, but the alphabetical sort
ensures `str` (01) is defined before `int.__str__` references it
(02), etc. The single concatenated parse means `int` can reference
`str` defined earlier in the file even though they'd be in
different files in the directory.

### 5.1 The `__pyc_insert_c_header__` directive

The first line of `00_runtime.py` is:

```python
__pyc_insert_c_header__('pyc_c_runtime.h')
```

This is a pyc compiler directive (see
[PYTHON_FRONTEND.md](PYTHON_FRONTEND.md) §11). It tells the C
backend to emit `#include "pyc_c_runtime.h"` at the top of the
generated `.c` file. Without this line, the C runtime header
wouldn't be referenced and emitted code wouldn't compile.

### 5.2 The base classes (`00_runtime.py`)

`__pyc_any_type__` is the root of the type lattice — the runtime
analog of IFA's `sym_any`. Every user class inherits from it
(transitively).

`object` is the direct user-visible base class. Implements
`__null__`, `__pyc_more__`, `__str__`, `__bool__`, `__len__`,
`__pyc_to_bool__`. Python user classes that inherit from `object`
get these for free.

`__pyc_None_type__` is the type of `None`. Implements `__bool__`
(returns False), `__pyc_to_bool__`, `__str__`, `__null__`. The
singleton `None` instance is `sym_nil` at IF1 level.

`bool` extends `object`. Methods include `__bool__`, `__int__`,
`__str__`, comparison operators.

`__base_iter__` is the abstract iterator interface: declares
`__iter__`, `__next__`, etc. Concrete iterators (`__str_iter__`,
`__list_iter__`, etc.) inherit.

### 5.3 The numeric types (`02_numeric.py`)

`int` and `float` define all the dunder methods (`__add__`,
`__sub__`, `__mul__`, ...). Each method body uses
`__pyc_operator__` and `__pyc_primitive__` calls to expose the
underlying primitive operations:

```python
class int:
  def __add__(self, other):
    return __pyc_operator__(self, __pyc_symbol__("+"), other)
```

These desugar to the matching `prim_add` SEND at compile time. The
analysis sees the user-level `+` and resolves to the int's
`__add__`, which in turn resolves to `prim_add`. After cloning +
inlining, the call collapses to a direct `_CG_prim_add(a, b)` in
the emitted C.

This is the **standard pattern** for adding new dunder methods:
1. Define the method in the right `__pyc__/0X_*.py`.
2. The body uses `__pyc_operator__` or `__pyc_primitive__` for the
   underlying primitive.
3. The analysis + cloning + inlining collapse the indirection.

### 5.4 `dict` (`07_dict.py`)

The dict implementation. Recently added; the project memory
mentions dict as parsed-only previously. Now also has a runtime.
Check the file directly for the API; the structure follows
`list`/`tuple` from `04_sequence.py`.

### 5.5 The fallback `__pyc__.py`

A single-file version of all the above. Maintained for cases where
`__pyc__/` isn't installed. Currently might lag the directory
version — the directory is the source of truth. If you find a
discrepancy, the directory wins.

---

## 5. The compat shim (`pyc_compat.py`)

```python
__pyc_declare__ = None
```

That's the whole file. Programs that use the `__pyc_declare__`
directive write:

```python
from pyc_compat import __pyc_declare__
__pyc_declare__('field_name', SomeType)
```

Under CPython, `__pyc_declare__` is `None` and the call no-ops with
a TypeError (which the user catches or programs work around). Under
pyc, the `from pyc_compat import` is recognised by
`build_import_syms` and *skipped* (see
[PYTHON_FRONTEND.md](PYTHON_FRONTEND.md) §6.5) — the import doesn't
actually load `pyc_compat.py`. Instead, `__pyc_declare__` is
resolved to the pyc-side `sym_declare` Sym (created in
`build_builtin_symbols`).

This is the standard pattern for any pyc-only feature you want
users to access via standard Python `import` syntax:
1. Add the name to `pyc_compat.py` as a no-op constant.
2. Recognise the import in `build_import_syms` and skip.
3. Resolve the name in `build_environment` to a frontend-side Sym.
4. Handle the SEND specially in `build_builtin_call_pyda` or
   register a primitive transfer function.

---

## 6. The `pyc_symbols.h` macro table

(See [PYTHON_FRONTEND.md](PYTHON_FRONTEND.md) §9 for the full
treatment.)

The macros `S(name)`, `P(name)`, `B(name)` are expanded across
several frontend files to declare/define/register the pyc-specific
Syms that the runtime and builtin module reference. New Sym names
need an entry here.

---

## 7. The build → link pipeline

When pyc compiles `hello.py`:

```
hello.py
  ↓ pyc frontend
hello.py.c   (with #include "pyc_c_runtime.h" at top)
  ↓ Makefile.cg (via c_codegen_compile)
cc -O ... -I$system_dir hello.py.c -o hello -lgc -lm
  ↓
hello   (binary linking against libgc, libm)
```

`Makefile.cg` (at `$system_dir/Makefile.cg`) sets:
- `CG_ROOT=$system_dir` — for finding `pyc_c_runtime.h`.
- `-I$CG_ROOT` — adds the include path.
- `-lgc` — Boehm GC.
- `-lm` — standard math.
- `-O` if `OPTIMIZE=1` (from pyc's `-O` flag).
- `-g` if `DEBUG=1` (from pyc's `-g` flag).

The user's `hello.py` doesn't need to know about any of this. The
runtime is statically linked via header inclusion; the GC is
dynamically linked.

---

## 8. Adding new runtime support

Concrete recipes for common changes:

### 9.1 New built-in function (e.g., `min`)

1. Add `def min(...)` to `__pyc__/05_builtins.py` using the
   `__pyc_*` primitives for the impl.
2. If it needs a new primitive, follow recipe 9.2.
3. Rebuild pyc.
4. Add a test in `tests/`.

### 9.2 New compiler primitive (e.g., `__pyc_sqrt__`)

1. Add `B(__pyc_sqrt__)` to `pyc_symbols.h`.
2. Add `prim_reg(sym___pyc_sqrt__->name, sqrt_transfer_function,
   sqrt_codegen)->is_visible = 1;` to
   `python_ifa_main.cc:add_primitive_transfer_functions`.
3. Implement `sqrt_transfer_function(pn, es)` to refine the result
   type to `sym_float64`.
4. Implement `sqrt_codegen(fp, n, f)` to emit `result = sqrt(arg);`.
5. Add `#include <math.h>` (already in `pyc_c_runtime.h`).
6. Use from Python via `__pyc_primitive__(__pyc_symbol__("__pyc_sqrt__"),
   x)`.

See [PRIMITIVES.md](PRIMITIVES.md) §11 for the full recipe.

### 9.3 New runtime type (e.g., set)

1. Define the class in `__pyc__/0X_*.py` (numbered to control load
   order).
2. Add C-level layout via the `__pyc_insert_c_header__` directive
   pointing to a new header (or extend `pyc_c_runtime.h`).
3. Add allocation primitive `_CG_prim_set(...)` to the runtime
   header.
4. Add transfer functions + codegen for any new compiler primitives
   the class uses.
5. Add `Sym *sym_set = 0;` etc. and register in
   `build_builtin_symbols`.
6. Test with `tests/set_basic.py`.

### 9.4 New runtime helper (e.g., a C function `_CG_my_helper`)

If the helper is small and inline:
1. Add `static inline ... _CG_my_helper(...)` to
   `pyc_c_runtime.h`.

If the helper is large or has dependencies:
1. Add the prototype to `pyc_c_runtime.h`.
2. Add the implementation to a new `.c` file linked at compile time
   (extend `Makefile.cg`).

---

## 9. Gotchas

### 10.1 `__pyc__/` is concatenated, not imported per-file
A name defined in `01_str.py` is visible throughout `02_numeric.py`
*because they're parsed as one module*. If you split a file in a
way that puts a definition AFTER a reference, the parse will fail
with "name not defined." Renumber to fix.

### 10.2 The `__pyc__.py` fallback may be stale
If you edit `__pyc__/*.py` but not `__pyc__.py`, builds with
`__pyc__/` present work; builds without (rare but possible) use the
stale fallback. Either keep both in sync or delete the fallback if
the directory is always available.

### 10.3 `pyc_compat.py` is auto-skipped, not auto-supplied
The frontend skips `from pyc_compat import X` but doesn't create the
underlying Sym automatically. If `X` isn't already in
`pyc_symbols.h` + `build_environment`, the user gets "name not
defined." Always wire the Sym side too.

### 10.4 String length is in the 8 bytes BEFORE the pointer
`_CG_string_len(s)` reads `*(int64*)(s - 8)`. If you allocate a
string via `MALLOC` without leaving 8 bytes of header space, length
queries will read garbage memory. Always use the `_CG_String(...)`
macro for allocation.

### 10.5 Boehm GC requires `MEM_INIT` before anything
`MEM_INIT()` (= `GC_INIT()`) must be the first call in `main`.
The emitted main always calls it; if you're embedding pyc-compiled
code in another C program, the call is your responsibility.

### 10.6 `_CG_Symbol(id, name)` discards the name
The macro is `((void *)(uintptr_t)_x)` — only the integer id makes
it to runtime. The name is purely for the human reader of the
generated C. Don't try to introspect symbol names at runtime.

### 10.7 `True` / `False` / `null` are macros
`#define True 1` etc. These are global names in user-emitted C. If
your user Python code defines a local `True`, the codegen may
shadow it inconsistently. Don't shadow builtins.

### 10.8 The `_CG_*` macros expect specific arg types
`_CG_prim_add(a, b)` expands to `((a) + (b))` — works for any C
numeric. But `_CG_prim_period(o, sel)` is more constrained — `o`
must be a struct pointer with the expected layout. Don't call macros
with wrong types; the C compiler errors will be cryptic.

### 10.9 Adding to `pyc_c_runtime.h` requires rebuilding ALL pyc tests
Header-only changes don't rebuild `pyc` itself (the compiler
binary) but DO require recompiling every `tests/*.py.c` output.
Run `make clean && make test_pyc` after runtime changes.

### 10.10 `7_dict.py` is recent
The `dict` implementation was added after the original 7-file
layout. If older docs/memory refer to "7 files," that's now 8.
Don't assume the count is fixed.

### 10.11 The `__pyc__.py` synthetic filename
When loading via `dparse_builtin_dir`, the synthetic filename
`<system_dir>/__pyc__.py` is used for path resolution (e.g.,
locating `pyc_c_runtime.h`). This means the directory and the
flat file share the *same* synthetic name — which is intentional
so error messages reference a consistent path.

---

## 10. Symptom → start-here

| Symptom | Start here |
|---|---|
| "undefined reference to `_CG_*` at link time" | Missing function in `pyc_c_runtime.h`; add as `static inline` or as `.c` impl |
| "name `bytearray` not defined" | `__pyc__/06_bytearray.py` not loaded; check `IFA_SYSTEM_DIRECTORY` |
| "string length wrong" | `_CG_string` constructor didn't write the 8-byte length header |
| "GC crash on first allocation" | `MEM_INIT()` not called |
| "`from pyc_compat import X` produces undefined X" | Sym not wired; add to `pyc_symbols.h` + `build_environment` |
| "forward reference in `__pyc__/` fails" | Wrong numeric prefix; rename to enforce load order |
| "stale `__pyc__.py` (single file) used" | Either delete the file or update it alongside the directory |
| "`__pyc_insert_c_header__` had no effect" | The directive only works in builtin module load; user files can use `__pyc_insert_c_code__` instead |
| "different output between OPTIMIZE=1 and =0" | C compiler issue, not pyc; reproduce with hand-written C |
| "binary aborts in Boehm GC init" | Wrong libgc version; try `apt-get install libgc-dev` |

---

## 11. References

- `pyc_c_runtime.h` — the runtime header.
- `pyc_compat.py` — the import shim.
- `pyc_symbols.h` — the macro table for frontend-known Sym names.
- `__pyc__/*.py` — the Python builtin module.
- `__pyc__.py` — single-file fallback.
- `Makefile.cg` (in install dir) — driver for the C compile.
- Sister docs: [PYTHON_FRONTEND.md](PYTHON_FRONTEND.md) (the
  frontend that consumes the builtins),
  [ifa/CODEGEN_C.md](ifa/CODEGEN_C.md) (the backend that emits
  `_CG_*` calls), [ifa/PRIMITIVES.md](ifa/PRIMITIVES.md) (the
  primitive system this runtime implements).
