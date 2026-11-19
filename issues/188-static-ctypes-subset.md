# 188 — a static subset of `ctypes`: calling C libraries from pyc programs

**Status:** open. The core landed 2026-11-11, and step 0 and item 2 on
2026-11-18 (`python_ifa_ctypes.cc`, `pyc_lib/ctypes/`, tests `ctypes_*`);
what is left is the list under
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
| `POINTER(T)` | a generated pointer class per interned pointee type (a scalar, `S`, or a pointer to one): nominal, so `POINTER(S)` and `POINTER(T)` do not mix. A pointer object is a one-word C cell, as in CPython |
| `c_int(5)`, `x.value` | a box: a generated class over a one-scalar C cell, one per canonical class (`c_int32` is `c_int`), shared by every module through the builtin module |
| `sizeof(T)` | the C compiler's `sizeof`; `sizeof(x)` of an instance is a run-time method |
| `byref(s)`, `pointer(s)` | run-time functions in `pyc_lib/ctypes` |

Three decisions carry the design:

- **Argument checking is method dispatch.** Each wrapper converts its
  arguments with a method only the acceptable types define
  (`a.__pyc_ffi_c_int__()` exists on `int`, `bool` and a `c_int` box, not
  on `float` or a `c_long` box), so a
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
| a `bytes` of length other than 1 passed where a `c_char` is declared | a run-time `TypeError`, where CPython raises `ArgumentError` (whose message carries the same `TypeError` text). As a box value or a field, both raise `TypeError`. |

## Refused, with the line

A library that is not a string literal, `None` or `find_library("lit")`; a
function called with no `argtypes` (pyc does not guess a signature from the
arguments); a declaration that is not at module top level; and any other use
of a ctypes name (`cast`, `CFUNCTYPE`, `c_wchar`, constructing a pointer
from a value: write `pointer(x)`).

## Not done

1. **Callbacks** (`CFUNCTYPE`). A pyc function is cloned per contour and C
   needs one entry point, so the declared signature has to seed flow
   analysis as an external caller. Anything a library retains
   (`user_data`) then needs pinning, because Boehm does not scan the
   library's heap.
2. ~~**Scalar boxes and out-parameters**~~: done 2026-11-18, see Item 2 under the plan below. `p[i]` on a
   NULL pointer does not reach an `except` yet: that is
   [175](175-an-exception-raised-in-an-operator-method-does-not-propagate.md)'s
   item-subscript half. `p.contents` on NULL does.
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

## Plan for items 1–3 (2026-11-18)

Order: **step 0, then 2, then 3, then 1.** Steps 0, 2 and 3 are frontend
only (`python_ifa_ctypes.cc`, `pyc_c_runtime.h`), like the core. Item 1 is
the only one that needs ifa: a second FA entry point and a fixed C entry
point in both backends. Rough size: step 0 a day, item 2 two days, item 3
two days, item 1 about a week.

Every behaviour below was checked against CPython 3 on 2026-11-18. Each
step's test runs under CPython and pyc on both backends, and under
`--strict`. (A `tests/*.ffi.cc` is the LLVM backend's generated shim, written
beside the program, not a golden; three were committed with the core and
are untracked and ignored since step 0.)

### Step 0 — one model for C-layout objects (done 2026-11-18)

Items 2 and 3 both need things that the current `FType` (a flat kind plus
`FStruct *`) cannot say: a pointer to a scalar, a pointer to a pointer, an
array of structs. They also need a pointer object whose own value lives in
C storage, because `byref(p)` must be the address of `p`'s cell.

- Make `FType` a tree, `{kind, c, FStruct *s, FType *elem, int n}`, with
  the new kinds `K_BOX` (a scalar box, `c_int()`), `K_ARRAY` (`elem * n`)
  and `K_PTR` over any `elem`. Intern the types by structure, so each
  distinct type has one id and one generated class: CPython caches these
  too (`POINTER(c_int) is POINTER(c_int)`, `(c_int * 4) is A`). Structs
  stay nominal, one per class.
- Every C-layout object gets the representation a Structure has today:
  `_buf` (collected, scanned storage from `_CG_ffi_buf`) and `_addr`. A
  view (`p.contents`, `t.s`, `a[i]` of a struct) is the same class with
  `_buf` set to the owner's buffer, so it keeps the owner alive.
- **A pointer object becomes a one-word cell**: `_buf` holds the address
  and `_addr` points at the cell. `p.contents` reads the cell. The `_keep`
  field goes away, because a pointee that pyc allocated is now reachable
  from a scanned cell. NULL is still a falsy instance
  (`bool(POINTER(c_int)())` is False).
- The shim emits `sizeof` and `offsetof` per type (`_CG_ffi_sizeof_K`,
  `_CG_ffi_off_K_f`) and a generic read and write per scalar C type, in
  place of today's per-field getter and setter. That keeps the shim linear
  in the number of types rather than in the number of fields.

**Done when** the existing `ctypes_*` tests pass unchanged.

**What landed.** `FType` carries `elem` and `id`; `ptr_to` interns
`POINTER(T)` by structural key, and every Structure interns its own pointer
type when its class is read. Each pointer type generates, after its root
Structure: `__pyc_ffi_P_K__` (the cell; `contents` and `p[i]` when the
pointee is a Structure with fields or a pointer, `byref` and `pointer` when
`POINTER(POINTER_K)` is itself declared), `__pyc_ffi_R_K__` (`byref`'s
result), `__pyc_ffi_mk_K__(addr)` (a new cell holding `addr`: a foreign
result, `pointer(x)`) and `__pyc_ffi_pview_K__(addr, keep)` (a cell it does
not own). The `__pyc_ffi_as_K__` conversion is keyed by the pointer type's
id, no longer by the Structure's. The base `__pyc_ffi_pointer__` allocates
the cell; the runtime gained `_CG_ffi_rd_ptr`, `_CG_ffi_wr_ptr`,
`_CG_ffi_sizeof_ptr`. `_keep` is gone from pointers.

- A pointer FIELD now reads as a view of the field's cell (`_CG_ffi_off_K_f`
  is emitted for pointer fields), where it used to read as a copy of the
  address. That is CPython's behaviour: `a.next.contents.v = 20` writes
  through, and `pointer(a.next)` points at the field.
- `POINTER(T)()` and a pointer alias called with no arguments construct a
  NULL pointer (`rewrite_ptr_ctors`, between scanning and generation), as
  `POINTER(GError)()` needs. Constructing one from a value is refused.
- **Not done, deliberately:** scalar fields keep their per-field shim getter
  and setter. A generic read at `addr + offsetof` is two calls per access
  where it is now one, and on the LLVM backend the shim is a separate
  translation unit, so neither inlines. Item 2's boxes and item 3's arrays
  add the generic per-scalar accessors where they need them.

`tests/ctypes_pointers.py` covers a self-referential `Node`, a pointer field
as a view, `POINTER(POINTER(Node))`, `byref(p)` and a pointer-to-pointer
handed to libc's `memcpy`, NULL construction and truthiness, and NULL
`contents` raising `ValueError`, identical to CPython on both backends and
under `--strict`.

### Item 2 — scalar boxes, out-parameters, `POINTER(scalar)` (done 2026-11-18)

The surface: `x = c_int(5)`, `x.value`, `x.value = 3`, `byref(x)`,
`pointer(x)`, `POINTER(c_int)` as an argument type, a result type and a
field type, and `POINTER(POINTER(S))`. That last one is the `GError **`
idiom: `err = POINTER(GError)(); f(..., byref(err)); if err: err.contents.message`.

- `c_T(v)` stops being a refused leftover and becomes construction of the
  box class `__pyc_ffi_box_K__`. It has a `value` property, with the store
  side through the existing setter dispatch (`pyc_add_setter_name`), and the
  `__pyc_ffi_byref__` and `__pyc_ffi_pointer__` methods a Structure has.
- CPython accepts a box where its scalar is declared (`f(c_int(3))` for a
  `c_int` argument). Give the box the same conversion method the scalar
  has (`__pyc_ffi_int__`, `__pyc_ffi_float__`, `__pyc_ffi_charp__`).
- `POINTER(T)` for any `T`, via step 0. As in CPython, `p.contents` is a
  box view and `p[i]` is the **value** (`type(p[0])` is `int`, and
  `type(p.contents)` is `c_int`).
- Argument conversion: a `POINTER(T)` parameter accepts `byref(x)`,
  `pointer(x)`, a `POINTER(T)` instance, `None`, and, once item 3 lands,
  an array of `T`. Each one gets `__pyc_ffi_as_K__`, so a mismatch is still
  a compile-time dispatch error.
- Add `c_char` and `sizeof(T)` to the scalar table and the static names.
  Item 3 needs `c_char`, and every callback example needs `sizeof`.
  `sizeof` is a compile-time `__pyc_c_call__` of `_CG_ffi_sizeof_K`.
- A `c_void_p` box's `.value` is the same `int`-or-`None` union as a
  `c_void_p` result, so it falls under the existing `PYC_FFI_VOIDP_INT`
  gate and `--strict` refuses it.

**Test** `tests/ctypes_out_params.py` against libc and libm:
`frexp(12.0, byref(e))` gives `0.75 4`, plus `modf`, `strtol` with a
`POINTER(c_char_p)` end pointer, a box passed by value, `p[0]` against
`p.contents`, and NULL truthiness. The `GError **` shape gets a libc
stand-in or a hand run against glib, as the core's glib check was.

**What landed.**

- The scalar table carries each name's canonical class on an LP64 target
  (`c_int32`, `c_int`; `c_size_t`, `c_ulong`; `c_longlong`, `c_long`), and
  that is the type's identity: `POINTER(c_int64)` is `POINTER(c_long)`, as
  in CPython. `c_char` is new: a `bytes` of length 1 or an `int` in, a
  one-byte `bytes` out (`_CG_ffi_char`), NUL included.
- **A box converts only where its own class is declared.** CPython rejects
  `abs(c_long(3))` for a `c_int` parameter and accepts `abs(c_int32(3))`.
  So the per-kind conversions (`__pyc_ffi_int__`, `__pyc_ffi_float__`) became
  one method per canonical class, `__pyc_ffi_c_int__`, defined on `int`,
  `bool`, `float` and `bytes` as CPython accepts them, and on that class's
  box. A mismatch is still a compile-time dispatch error, now naming the C
  type (`tests/ctypes_arg_type_refused.py.check`).
- The box classes (`__pyc_ffi_B_K__`), their views, and every pointer type
  whose root is a scalar are generated into the **builtin module**, so every
  module shares one `c_int`. The shim gets a generic read, write and
  `sizeof` per box type (`_CG_ffi_rd_sK`, `_wr_sK`, `_sizeof_sK`), which is
  the generic accessor step 0 deferred. Two frontend changes follow from
  generating property-bearing classes there:
  - the `@property` scan does not visit the builtin module, so the
    generated classes write their getters out as `__pyc_get_NAME__` and
    register the name (`pyc_add_property_name`);
  - `inject_property_accessors` no longer appends a default accessor to a
    builtin class that defines that accessor itself. It used to append a
    second `__pyc_get_value__` after the box's own, and the later one wins.
- The lowering arms `pyc_program_has_raise` when it generates box or
  pointer classes into the builtin module. Their raises are reached through
  `p.contents` and `b.value`, which the syntactic gate cannot see in builtin
  code.
- `p.contents = x` repoints `p` (`__pyc_set_contents__`), for a struct, a
  box and a pointer pointee. `p[i] = v` stores through a scalar pointer.
- `POINTER(c_void_p)` and a `c_void_p` box fall under `PYC_FFI_VOIDP_INT`,
  so `--strict` refuses them, as planned.

Not done: `POINTER(T)(x)` (write `pointer(x)`), a box's `from_param`,
`c_wchar`, and keyword arguments to a box constructor. The `GError **`
shape is `tests/ctypes_pointers`' `POINTER(POINTER(Node))` with `byref`,
which libc's `memcpy` writes; it was not run against glib.

### Item 3 — arrays, and a Structure held by value in a field

The surface: `A = c_int * 4`, `A(1, 2)` (the rest zero), `a[i]`,
`a[i] = v`, `len(a)`, iteration, `list(a)`, an array as an argument (it
decays to a pointer), an array field (`("v", c_int * 3)`), a struct field
(`("s", S)`), and `c_char * N`.

- `T * N` needs a literal `N`, or a module-level name bound to a literal,
  read at lowering time. Anything else is refused with its line, as a
  non-literal library is.
- `__pyc_ffi_arr_K__` gets `__init__(*items)`, `__len__`, `__getitem__`
  and `__setitem__` bounds-checked with `IndexError("invalid index")` as
  CPython raises, `__iter__`, and `__pyc_ffi_as_K__` for the decay to
  `POINTER(elem)`. A struct element reads as a view, a scalar element as a
  value.
- Fields: the shim struct emits `int v[3];` or `struct _CG_ffi_s_K s;`.
  The getter returns a view at `self._addr + offsetof(...)` that shares
  `self._buf`, so `a2 = t.s; a2.q = 9` changes `t`, as CPython's does.
  The setter copies the bytes (`t.s = S()` is a copy). Delete the "by value
  is not supported yet" refusal in `set_fields`.
- `c_char * N` is CPython's special case: as a field it reads as `bytes`
  up to the first NUL, and an array of it has `.value` and `.raw`. Do it
  in this step, because C structs are full of `char name[32]`.
- A slice of an array (`a[1:3]`) gives a list in CPython. That is cheap to
  add, but it can be refused at first.

**Test** `tests/ctypes_arrays.py`: construction, indexing, the
out-of-range `IndexError`, an array field and a struct field showing the
aliasing view and the copy on store, `c_char * 8` reading as `bytes`, and
an array passed to `memset` or `memcpy`.

### Item 1 — callbacks (`CFUNCTYPE`)

The target is the CPython ctypes documentation's own example, verified
above:

```python
CMP = CFUNCTYPE(c_int, POINTER(c_int), POINTER(c_int))
def cmp(a, b): return a[0] - b[0]
libc.qsort(arr, len(arr), sizeof(c_int), CMP(cmp))
```

Three problems, each with its own mechanism.

**(a) C needs one fixed entry point with the declared signature.** A pyc
function is named `_CG_f_<symid>_<i>` and its C signature drops dead and
constant arguments (`write_c_fun_proto` emits only live ones), so the shim
cannot call it directly. The lowering generates one pyc trampoline per
`CFUNCTYPE` type, `def __pyc_ffi_cbt_K__(cb, a0, a1): ...`. It converts
each word argument into the Python value the callback expects (an `int`, a
`float`, `bytes` through `_CG_ffi_cstr`, a `POINTER` cell), calls
`cb.fn(...)`, and converts the result back. The trampoline is marked
**exported** under a C name. That is a generic ifa feature, not a Python
one (`Sym::export_name`, a header change, so `make clean`). Both backends
then emit `extern "C" <word> _CG_ffi_cb_K(<word>...)` with the full
declared arity, forwarding to the live arguments of the trampoline's single
clone: `cg.cc` for C, and `llvm.cc` / `cg_emit_llvm.cc` for LLVM.

**(b) FA has to see the trampoline as called, with the declared types.**
FA has one root today (`make_top_edge` / `refresh_top_edge`, `fa.cc` around
line 3492). Add one root edge per exported function, with each formal
filtered to its declared type: the machine-word `int` or `float`, and the
callback object's class for `cb`. That is a genuine external caller, which
is what the item asks for. The alternative is a call from `main` behind an
opaque false guard. It needs no ifa change, but it emits dead code and is
the kind of structural workaround the directives warn against, so use it
only as a stepping stone. The trampoline is only ever reached from its
root, so FA has no demand that would split it. If it ever has two clones,
that is an internal error naming the demand, not something to work around.

**(c) A C function pointer cannot carry a closure: libffi closures**
(decided 2026-11-18, over a fixed trampoline pool). `CB(f)` builds an
instance of `__pyc_ffi_CB_K__`, a collected object holding `fn`, and
allocates a libffi closure for it (`ffi_closure_alloc` +
`ffi_prep_closure_loc`, as CPython does). Its `ffi_cif` is built once per
`CFUNCTYPE` type from the declared C types, which the lowering knows
statically. The closure's handler is a shim function that reads the C
arguments from libffi's `args[]`, widens them to words, and calls
`_CG_ffi_cb_K(cb, ...)`. The closure's `user_data` is the CB object, held as
a pointer Boehm does **not** see (libffi's closure memory is not scanned), and
a finalizer on the CB object frees the closure. So the callback lives exactly
as long as the program keeps the CB object, which is CPython's rule (calling
a collected callback is undefined behaviour there too). `fn` is an ordinary
field, so FA sees every callable stored at every `CB(f)` site and dispatches
over them. There is no limit on how many callbacks are alive at once.

The cost is a library: the generated program links `-lffi`, and **only** when
it declares a `CFUNCTYPE`, through `codegen_link_libs` as the `CDLL` entries
are. `make PORTABLE=1 dist` must bundle a static `libffi.a` in `lib/pyc`, as
it bundles Boehm's, and `dist_smoke.sh` must gain a callback program so the
self-containment check covers it. CI installs `libffi-dev` (ubuntu) and
`libffi` (Homebrew).

What else follows from these:

- **A callback that raises.** No pyc exception may unwind through C. The
  trampoline catches everything, prints CPython's `Exception ignored on
  calling ctypes callback function: <function ...>` and the exception to
  stderr, and returns zero. Nothing reaches the caller, as in CPython.
- **Retention (`user_data`).** A library that keeps the function pointer
  needs the program to keep the CB object, as in CPython. A Python object
  passed as `user_data` needs `py_object`, which is out of scope. The
  supported shape is a `c_void_p` of `None` or the address of a ctypes
  object the program keeps. Pinning a pointer handed to C is
  `GC_malloc_uncollectable` storage, and that waits for a program that
  needs it.
- **Threads.** A callback from a thread the library created is refused in
  documentation and not detected; pyc does not register foreign threads
  with Boehm.
- A `CFUNCTYPE` restype or argtype of `c_void_p` or `c_char_p` follows the
  existing result rules (the `PYC_FFI_VOIDP_INT` gate).

**Test** `tests/ctypes_callbacks.py`: the `qsort` example, `bsearch`, two
`CB` objects alive at once, a closure capturing a local, a
raising callback (its stderr goes in a `.check` for the "Exception ignored"
line), and `--strict`. A `--jit` run stays refused (item 6).

**Stop conditions, written before starting:**

- If the exported trampoline ever gets more than one clone on the corpus or
  the suite, stop and find the demand. Do not pick one clone.
- If the extra FA root changes any corpus program's contour count or
  verdict, it is not inert. Every program that does not import ctypes must
  be unaffected, so check it with a `check` sweep before and after.

## Verification

`tests/ctypes_scalars`, `ctypes_struct`, `ctypes_bindings` pass on both
backends and are byte-identical to CPython, also under `--strict`. Four
`ctypes_*_refused` tests pin the diagnostics. glib itself (`GString`,
`GList`) was run by hand against `libglib-2.0.so.0`; it is not in the suite
because CI may not have the library.
