# PYTHON_FRONTEND — pyc Python Front End

A working reference for `pyc.cc` + the `python_*.cc` family — the
Python-specific frontend that lowers Python source into IFA's IF1 IR.
The IFA library on the back end is documented in
[ifa/ARCHITECTURE.md](ifa/ARCHITECTURE.md) and friends. For the
end-to-end flow, see [PIPELINE.md](PIPELINE.md).

---

## 1. In one paragraph

The pyc frontend has three layers. First, `python_parse.cc` + `python.g`
parse `.py` files with DParser into a `PyDAST` tree (the
language-specific AST). Then `python_ifa_*.cc` runs two passes over the
AST: `build_syms` populates the symbol table and scope chain, and
`build_if1` emits IF1 `Code` trees. The frontend never calls into the
analysis core itself — it only registers symbols, builds IR, and
implements the `IFACallbacks` interface so the analysis can call back
when it needs language-specific information (default-arg wrappers,
violation-driven retries, codegen-prologue lines).

---

## 2. File map

```
pyc.cc                       ← CLI / orchestration
python_parse.{cc,h}          ← DParser → PyDAST entry points
python.g                     ← Python 3 grammar (compiled to python.g.d_parser.cc)
python.g.d_parser.cc         ← generated parser tables (13840 lines)
python_ast.h                 ← PyDAST node definitions (PyASTKind, PyOp, PyCtx)
python_ifa.h                 ← PycCallbacks/PycSymbol/PycAST/PycModule (public)
python_ifa_int.h             ← PycCompiler + PycScope + sentinels + cross-file decls (internal)
python_ifa_util.cc           ← globals, PycSymbol/PycAST methods, ast_html (~139 lines)
python_ifa_sym.cc            ← Sym creation, scope mgmt, builtin syms (~359 lines)
python_ifa_build_syms.cc     ← symbol-table-building pass (~725 lines, pyda-only)
python_ifa_build_if1.cc      ← IF1-code-generation pass (~1249 lines, pyda-only)
python_ifa_main.cc           ← ast_to_if1 entry, primitive registration, build_environment
defs.h                       ← top-level includes
pyc_symbols.h                ← S(name)/P(name)/B(name) macro list of pyc-specific syms
pyc_c_runtime.h              ← runtime C helpers emitted alongside generated code
pyc_c_runtime_plib.h         ← plib variant
pyc_compat.py                ← runnable-under-CPython shim for __pyc_declare__ etc.
__pyc__/                     ← builtin Python module (str, int, list, ...) as 7 ordered files
__pyc__.py                   ← legacy single-file builtin (fallback if __pyc__/ absent)
```

---

## 3. Lifecycle

A single `./pyc file.py` invocation:

```
main()                                pyc.cc:138
├─ process_args / init_system / init_config / init_logs / Service::start_all
├─ if --dparse_only:   dparse_python_file(*) exit
├─ if --dparse_ast:    dparse_python_to_ast(*) + pyast_print(*) exit
│
├─ for i = -1 .. nfiles-1:                                 [pyc.cc:154]
│     i == -1: load __pyc__ (directory or .py)
│     else:    load user file
│   build a PycModule for each
│
└─ if any modules loaded:
    ast_to_if1(mods)                  python_ifa_main.cc:206
    │  ifa_init(PycCompiler)          → new IF1; new PDB; init_ast
    │  partial_default = Partial_NEVER
    │  build_builtin_symbols()
    │  add_primitive_transfer_functions()
    │  build_search_path(ctx)         ← PYTHONPATH walk
    │  build_environment(mods[0])     ← scope_sym for every builtin
    │  for x in mods:  build_syms(x, ctx)
    │  finalize_types(if1)
    │  for x in mods:
    │      build_module_attributes_if1(x, ctx, &code)
    │      build_if1_module_pyda(x->pymod, ctx, &code)
    │  finalize_types(if1)
    │  if test_scoping: exit(0)
    │  build_init(code)               ← wraps __main__ with reply primitive
    │  build_type_hierarchy()
    │  fixup_aspect()                 ← resolve super dispatch types
    │
    compile(filename)                 pyc.cc:81
       ifa_analyze → ifa_optimize → backend  (see PIPELINE.md)
```

There are two passes over each module's `PyDAST` tree:

1. **`build_syms`** (`python_ifa_build_syms.cc:675`) — creates `Sym`s for
   every binding, populates `PycScope::map`, resolves imports, sets up
   class/function scopes.
2. **`build_if1`** (`python_ifa_build_if1.cc:1259`) — emits `Code` trees
   (`if1_send`/`if1_move`/...) using the symbols from pass 1.

Both passes share the same `PycCompiler` and walk the *same* PyDAST tree
(via `getAST(PyDAST*, ctx)` which interns a `PycAST` per `PyDAST` node).

---

## 4. Data types

### 4.1 `PyDAST` (`python_ast.h`)

DParser AST node. One per syntactic construct. Fields:

```c
PyASTKind kind;
Vec<PyDAST *> children;
cchar *str_val;          // PY_name, PY_string, PY_number, PY_dotted_name
long int_val;            // integer literal
double float_val;        // float literal
bool is_int, is_imag;
int op;                  // PyOp (binop / cmp / unaryop)
int ctx;                 // PyCtx: PY_LOAD / PY_STORE / PY_DEL
cchar *filename;
int line;

// IFA annotations (filled in by build_syms / build_if1)
Code *code;
Label *label[2];
Sym *sym, *rval;
PyDAST *parent;
unsigned is_builtin : 1;
unsigned is_member : 1;
unsigned is_object_index : 1;
```

Note the IFA annotations live on `PyDAST` itself (set by the second pass)
*and* also on the wrapping `PycAST` (set by the first pass) — both paths
exist for historical reasons. The pyda-path consistently uses the
`PycAST` wrapper via `getAST(PyDAST*, ctx)`.

### 4.2 `PyASTKind` (subset of common kinds)

`PY_module`, `PY_funcdef`, `PY_classdef`, `PY_decorated`, `PY_decorator`,
`PY_suite` (block), `PY_expr_stmt`, `PY_assign`, `PY_augassign`,
`PY_return_stmt`, `PY_if_stmt`, `PY_while_stmt`, `PY_for_stmt`,
`PY_try_stmt`, `PY_with_stmt`, `PY_import_name`, `PY_import_from`,
`PY_global_stmt`, `PY_nonlocal_stmt`, `PY_call`, `PY_attribute`,
`PY_subscript`, `PY_compare`, `PY_binop`, `PY_unaryop`, `PY_ternary`,
`PY_bool_and/or/not`, `PY_lambda`, `PY_name`, `PY_number`, `PY_string`,
`PY_tuple/list/dict/set`, `PY_listcomp`, `PY_genexpr`, `PY_slice`,
`PY_parameters`, `PY_varargslist`, `PY_fpdef`, `PY_arglist`,
`PY_keyword_arg`, `PY_star_arg`, `PY_dstar_arg`, `PY_arg_default`,
`PY_testlist`, `PY_exprlist`, `PY_comp_for/if`.

Full list at `python_ast.h:11`.

### 4.3 `PyOp` and `PyCtx`

- `PyOp`: arithmetic (`PY_OP_ADD/SUB/MUL/...`), comparison
  (`PY_CMP_EQ/NE/...`), bool/bitwise (`PY_OP_BITOR/...`),
  unary (`PY_OP_UADD/USUB/INVERT`). Used by `PY_binop`, `PY_cmp_op`,
  `PY_unaryop`, `PY_augassign`.
- `PyCtx`: `PY_LOAD` / `PY_STORE` / `PY_DEL`. CPython-style load/store
  context for names and attributes.

### 4.4 `PycSymbol`, `PycAST`, `PycModule`, `PycCallbacks` (`python_ifa.h`)

Frontend subclasses of the IFA interfaces. See
[ifa/ARCHITECTURE.md](ifa/ARCHITECTURE.md) §IFACallbacks for the
contracts these implement.

```c
class PycCallbacks : public IFACallbacks { ... };  // thin shim; just a vtable anchor

class PycSymbol : public IFASymbol {
  cchar *filename;
  PycSymbol *previous;          // chain for redefinition (used at scope exit)
  // implements pathname/line/source_line/copy/clone
};

class PycAST : public IFAAST {
  PyDAST *xpyd;                 // the underlying DParser AST
  cchar *filename;
  PycAST *parent;
  Vec<PycAST *> children;
  Code *code;
  Label *label[2];
  Sym *sym, *rval;              // for names: sym = the var; for expressions: rval = result var
  unsigned is_builtin : 1;
  unsigned is_member : 1;
  unsigned is_object_index : 1;
};

class PycModule {
  PyDAST *pymod;                // root of the per-module AST
  cchar *filename, *name;
  PycSymbol *name_sym, *file_sym;
  PycCompiler *ctx;
  bool is_builtin;
  bool built_if1;
};
```

`PycAST` is interned per `PyDAST` via the `pydmap` global. The two
helpers in `python_ifa_int.h:92,103` both look up or create the wrapper:

```c
static inline PycAST *getAST(PyDAST *n, PycCompiler &ctx);
static inline PycAST *getAST(PyDAST *n, PycAST *a);
```

Use the right one — the `(n, ctx)` form sets `filename` from the current
compiler context; the `(n, a)` form inherits from a parent PycAST.

### 4.5 `PycCompiler` and `PycScope` (`python_ifa_int.h`)

`PycCompiler` (formerly `PycContext`) extends `PycCallbacks` and owns
all the per-compilation state:

```c
class PycCompiler : public PycCallbacks {
  cchar *filename;             // current source file
  int lineno;
  void *node;                  // current PyDAST node being walked
  PycModule *mod, *package;
  Vec<PycModule *> *modules;   // all loaded modules
  Vec<cchar *> *search_path;   // PYTHONPATH + cwd + subdirs
  Vec<PycScope *> scope_stack; // active scope chain
  Vec<cchar *> c_code;         // pending __pyc_insert_c_code__ blobs
  Map<void *, PycScope *> saved_scopes;  // node → scope (re-enter same scope on pass 2)
  Map<int, Sym *> tuple_types; // arity → tuple type cache
  Vec<PycScope *> imports;     // imported module scopes (searched below stack)

  // accessors:
  Sym *fun(), *cls();
  Label *&lbreak(), *&lcontinue(), *&lreturn(), *&lyield();
  bool in_class(), is_builtin();

  // PycCallbacks overrides:
  void finalize_functions();
  Sym *new_Sym(cchar *name);
  Fun *default_wrapper(Fun *, Vec<MPosition *> &defaults);
  bool reanalyze(Vec<ATypeViolation *> &);
  bool c_codegen_pre_file(FILE *);

  int run(Vec<PycModule *> &mods);
};
```

`PycScope`:

```c
struct PycScope {
  int id;
  Sym *in;                     // containing module/class/function
  Sym *cls, *fun;              // enclosing class / function
  Label *lbreak, *lcontinue, *lreturn, *lyield;
  Map<cchar *, PycSymbol *> map;
};
```

The scope stack grows on `enter_scope(ctx)` and shrinks on
`exit_scope(ctx)`. `saved_scopes` keys scopes by their originating
`PyDAST*` so the second pass (`build_if1`) can re-enter the *same*
scope objects rather than rebuilding them. This is why scope creation
in `enter_scope` first checks `saved_scopes`:

```c
PycScope *c = ctx.saved_scopes.get(ctx.node);
if (!c) { /* allocate, attach to parent, save */ }
ctx.scope_stack.add(c);
```

### 4.6 Scope sentinels (`python_ifa_int.h:121`)

`PycScope::map` stores `PycSymbol*` values, but four small integers
cast to `PycSymbol*` are reserved as scope-marker sentinels:

```
GLOBAL_USE     ((PycSymbol *)1)
NONLOCAL_USE   ((PycSymbol *)2)
GLOBAL_DEF     ((PycSymbol *)3)
NONLOCAL_DEF   ((PycSymbol *)4)
MARKED(x)      ((uintptr_t)(x) < 5)
```

When `make_PycSymbol` records that a name in the current scope refers to
an outer-scope `Sym`, it inserts one of these sentinels in the local
scope's map. `find_PycSymbol` checks `MARKED(value)` first and walks
*past* the local scope when it sees a sentinel.

**Critical guard** (project memory): when `scope_stack.n == 1`,
`scope_stack.last() == scope_stack.v[0]`. If `make_PycSymbol`'s
PYC_GLOBAL branch put a `GLOBAL_DEF` sentinel into `scope_stack.last()`,
it would corrupt the global scope. The guard:

```c
if (!explicitly && !(ctx.scope_stack.n == 1))
    ctx.scope_stack.last()->map.put(name, GLOBAL_DEF);
```

is in `python_ifa_sym.cc`'s `make_PycSymbol` (`PYC_GLOBAL` case). Don't
remove this guard.

### 4.7 The `pydmap` global (`python_ifa_util.cc:17`)

```c
Map<PyDAST *, PycAST *> pydmap;
```

The single source of truth for `PyDAST → PycAST` lookup. Shared across
modules and across both passes. Cleared only when a fresh
`PycCompiler` is constructed (which currently happens once per
invocation, so effectively never cleared).

---

## 5. The two passes

### 5.1 `build_syms` (`python_ifa_build_syms.cc:675`)

Entry per module:

```c
int build_syms(PycModule *x, PycCompiler &ctx) {
  x->ctx = &ctx;
  ctx.mod = x;
  ctx.filename = x->filename;
  if (!ctx.is_builtin()) import_scope(modules[0], ctx);
  build_module_attributes_syms_pyda(x, ctx);
  ctx.node = x->pymod;
  enter_scope(ctx);
  build_syms_pyda(x->pymod, ctx);
  exit_scope(ctx);
  return 0;
}
```

`build_syms_pyda` is a recursive descent over `PyDAST`. For each kind:
- `PY_funcdef` → `gen_fun_pyda` allocates a function Sym, enters a new
  scope, walks `varargslist` to create arg Syms, then recurses into the
  body.
- `PY_classdef` → `gen_class_pyda` allocates a class Sym, enters a
  class scope, recurses. Handles `@vector("s")` decoration (sets
  `cls->is_vector = 1`).
- `PY_decorated` → unwraps decorators; recognises `@vector("s")` and
  passes `vector_size` into `gen_class_pyda`.
- `PY_lambda` → similar to funcdef but uses `gen_lambda_pyda`.
- `PY_import_name` / `PY_import_from` → `build_import_syms` which uses
  `import_file` to recursively build_syms on the imported module.
- `PY_name` → `make_PycSymbol(ctx, name, PYC_USE)`.
- `PY_global_stmt` / `PY_nonlocal_stmt` → `PYC_GLOBAL` / `PYC_NONLOCAL`.
- Assignment targets → `PYC_LOCAL`.
- Compound statements (`PY_if_stmt`, `PY_while_stmt`, etc.) → recurse
  without scope changes (Python's scoping is function-level, not
  block-level).

The pass produces:
- One `PycSymbol` per binding.
- `PycScope::map[name] = PycSymbol*` or sentinel.
- Scope chain attached via `saved_scopes`.
- Class hierarchy (via `Sym::specializes`/`implements`).
- Default argument AST pointers (recorded in `gen_fun_pyda` for later
  retrieval by `finalize_function`).

### 5.2 `build_if1` (`python_ifa_build_if1.cc`)

Entry per module:

```c
int build_if1_module_pyda(PyDAST *mod, PycCompiler &ctx, Code **code) {
  ctx.node = mod;
  enter_scope(ctx);                       // re-enters the scope created in pass 1
  for (auto c : mod->children) {
    build_if1_pyda(c, ctx);
    if1_gen(if1, code, getAST(c, ctx)->code);
  }
  exit_scope(ctx);
  return 0;
}
```

`build_if1_pyda` is a giant switch on `PyDAST::kind`. For each node it:
1. Recurses into children first (post-order).
2. Builds the Code via `if1_*` builders, attaching to
   `getAST(n, ctx)->code`.
3. Sets `getAST(n, ctx)->rval` to the Sym holding the expression's value.

Major shapes it emits:
- `PY_name` → reads `find_PycSymbol`; sets `rval = sym->var` or similar.
- `PY_assign` → `if1_move(rhs->rval, lhs->sym)`.
- `PY_binop` (and `PY_compare`, `PY_augassign`) → translates the `op`
  field through `map_pyop_to_operator` into an `if1_send` of
  `__add__` / `__sub__` / etc.
- `PY_call` → may be a builtin call (`build_builtin_call_pyda`) or a
  regular `if1_send`.
- `PY_attribute` → `if1_send(if1, ..., sym_operator, obj, sym_period,
  attr_sym, result)`.
- `PY_subscript` → translates to `__getitem__` / `__setitem__` via
  `call_method`.
- `PY_if_stmt` / `PY_while_stmt` / `PY_for_stmt` → use `if1_if`,
  `if1_loop` builders. Labels for break/continue go through
  `ctx.lbreak()` / `lcontinue()`.
- `PY_return_stmt` / `PY_yield_stmt` → `if1_send(sym_primitive,
  sym_reply, fn->cont, fn->ret)`.
- `PY_lambda` / `PY_funcdef` / `PY_classdef` → already have Syms from
  pass 1; this pass emits their bodies.
- `PY_power` → handles `**` via repeated `__mul__` or specialised
  shape. Note the project-memory bug fix: `ast->rval = cur_val` must
  always update, not just when null (`build_if1_pyda` `PY_power`).

The pass produces:
- `Code` trees attached via `PycAST::code` / `PyDAST::code`.
- Each `Sym` for a function/closure gets its body via `if1_closure`.
- `c_code` accumulates `__pyc_insert_c_code__` blobs.

### 5.3 Module attributes

`build_module_attributes_syms_pyda` (pass 1) declares the per-module
`__name__` and `__file__` symbols. `build_module_attributes_if1`
(`python_ifa_main.cc:139`) emits the matching `if1_move` to populate
them when the module is initialised.

For the very first user module, `__name__` is set to `"__main__"`;
for others, the module's actual name.

### 5.4 `build_init` and `__main__`

After all modules' if1 has been built, `build_init` (`python_ifa_main.cc:34`)
finalises the top-level closure:

```c
fn = sym___main__;
fn->cont = new_sym();
fn->ret = sym_nil;
if1_send(if1, &code, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret);
if1_closure(if1, fn, code, 1, &fn);
```

The `sym_primitive sym_reply cont ret` send is how IFA represents
function return; it makes `__main__` cleanly exit by replying nil.

### 5.5 `fixup_aspect`

`sym->aspect` is set by code handling `super` to refer to the class
whose superclass should receive dispatch. `fixup_aspect`
(`python_ifa_main.cc:124`) replaces those forward references with the
actual MRO-next class via `s->aspect = s->aspect->dispatch_types[1]`.
Runs after `build_type_hierarchy` so `dispatch_types` is populated.
Called twice (after `ast_to_if1` and lazily for new syms via
`install_new_fun`).

---

## 6. Symbol creation and scoping

### 6.1 `new_PycSymbol` family (`python_ifa_sym.cc:3`)

The base allocator:

```c
PycSymbol *new_PycSymbol(cchar *name);
PycSymbol *new_PycSymbol(cchar *name, PycCompiler &ctx);  // sets filename
```

Each returns a fresh `PycSymbol` whose `sym` is registered with IF1.

The convenience wrappers:

| Function | Purpose |
|---|---|
| `new_sym(name, global)` | Generic; sets `nesting_depth = LOCALLY_NESTED` if not global. |
| `new_sym(ast, global)` | Anonymous; binds to AST. |
| `new_sym(ast, name, global)` | Named + AST. |
| `new_global(ast, name)` | Always `nesting_depth = 0`. |
| `new_fun(ast, fun)` | Mark a Sym as a function: set `is_fun`, allocate `ret`/`cont`. |

### 6.2 `find_PycSymbol` (`python_ifa_sym.cc:284`)

Walks the scope stack from top to bottom, honouring sentinels:
- `GLOBAL_USE` / `GLOBAL_DEF` → jump to the global scope (index ≤ 1).
- `NONLOCAL_USE` / `NONLOCAL_DEF` → keep looking up.
- Otherwise return the found symbol.

The `level` output records where it found the symbol;
`type` records whether the find was via an explicit (`global`/`nonlocal`)
declaration or implicit.

### 6.3 `make_PycSymbol(ctx, name, scoping)` (`python_ifa_sym.cc:314`)

The single point of declaration. `scoping` selects:
- `PYC_USE` — a read; promote to global/nonlocal sentinels in the local
  scope if we found it outside.
- `PYC_LOCAL` — an assignment; allocate a fresh PycSymbol unless one
  exists locally or was explicitly declared. Fails on
  global/nonlocal redefinition conflicts.
- `PYC_GLOBAL` — an explicit `global x` statement. Records `GLOBAL_DEF`
  in the local scope (subject to the §4.6 guard).
- `PYC_NONLOCAL` — an explicit `nonlocal x` statement.

### 6.4 `enter_scope` / `exit_scope` (`python_ifa_sym.cc:248,270`)

`enter_scope(ctx, in)`:
- If `saved_scopes` has a scope for the current `ctx.node`, reuse it
  (this is how pass 2 finds the scope chain pass 1 built).
- Otherwise create a new `PycScope`, inheriting `fun`/`cls` from the
  parent scope on the stack, overriding if `in` is a function or class
  Sym.
- Push onto `scope_stack`.

`enter_scope(PyDAST *n, ctx, in)` first sets `ctx.node = n` then calls
the above. Use this form when the node and scope key should be one and
the same.

### 6.5 Module imports (`python_ifa_build_syms.cc:7`)

`import_file(name, p, ctx)`:
1. Parse the imported file with `dparse_python_to_ast`.
2. Create a `PycModule` for it; add to `modules`.
3. Save current scope_stack and other context.
4. Run `build_syms` on the imported module — gets its own fresh scope
   stack.
5. Restore everything.

**Critical:** must use `Vec::move()` to save/restore `scope_stack`, not
the `n = 0` trick. Setting `n = 0` then pushing overwrites outer
scopes in `v[0]`. From project memory: this was a real bug.

`get_module(name, ctx)` looks up an already-loaded module by name.

`build_import_syms` (called from pass 1 for `PY_import_name` /
`PY_import_from`):
- Walks `ctx.search_path` looking for `<mod>.py`.
- Calls `import_file` if not already loaded.
- `pyc_compat` is special-cased and never actually imported.

`import_scope` plugs the imported module's scope into `ctx.imports`
so `find_PycSymbol` can find its bindings.

---

## 7. Builtin module: `__pyc__/`

The Python-level builtins live in `__pyc__/` as 7 ordered files:

```
00_runtime.py    __pyc_any_type__, object, __pyc_None_type__, bool, __base_iter__
01_str.py        str
02_numeric.py    int, float
03_slice.py      __slice_iter__, slice
04_sequence.py   __list_iter__, list, __tuple_iter__, tuple
05_builtins.py   abs, all, any, bin, exit, range, len, chr, ord, hex, isinstance, issubclass
06_bytearray.py  bytearray
```

Loading: `pyc.cc:main` checks for `IFA_SYSTEM_DIRECTORY/__pyc__` as a
directory; if present, `dparse_builtin_dir` concatenates the sorted
`.py` files into one buffer and parses as a single module (so forward
references work across files within the directory). If the directory
is absent, falls back to `__pyc__.py`. The synthetic filename
`<system_dir>/__pyc__.py` is used for path resolution of generated C
headers.

`dparse_builtin_dir` (`python_parse.cc:64`):
- `scandir` sorted.
- Skip non-`.py` files.
- Read into a single GC-managed buffer (must outlive the AST).
- Insert newlines between files.
- Single `dparse` call.

---

## 8. The `IFACallbacks` overrides

The frontend implements these (declared `virtual` in
`ifa/ifa.h`, overridden in `python_ifa_int.h` and defined in
`python_ifa_sym.cc` / `python_ifa_util.cc`):

| Override | Body | Purpose |
|---|---|---|
| `~PycCallbacks()` | `python_ifa_util.cc:27` | vtable anchor (out-of-line). |
| `new_Sym(name)` | `python_ifa_sym.cc:25` | Allocate a `PycSymbol`; bind `sym->asymbol`. |
| `finalize_functions()` | `python_ifa_sym.cc:172` | Per-Fun: extract `default_args` from `varargslist` and populate `fn->fun->default_args`. Handles `__new__` wrappers from `gen_class_pyda` (fn's `init` points to the matching `__init__`). |
| `default_wrapper(f, defaults)` | `python_ifa_sym.cc:186` | Build a wrapper Fun that supplies default args at the specified positions. |
| `reanalyze(violations)` | `python_ifa_sym.cc:216` | On NOTYPE violations for missing instance variables, add them to the receiver's class and return true to force another analysis pass. |
| `c_codegen_pre_file(fp)` | `python_ifa_sym.cc:241` | Emit accumulated `__pyc_insert_c_code__` blobs (`c_code` Vec) at the top of the generated `.c`. |

Defaults (`make_LUB_type`, `coerce`, `promote`, generics) are inherited
unchanged from `IFACallbacks`.

`PycSymbol::clone()` overrides `IFASymbol::clone` to use the IFA
machinery (`copy()` → `sym`).

---

## 9. The macro-driven builtin symbol list (`pyc_symbols.h`)

The header is included multiple times with different `S(_x)` /
`P(_x)` / `B(_x)` definitions:

- `S(name)` — declares `extern Sym *sym_##name`; populates a builtin.
- `P(name)` — same plus `scope_sym(ctx, sym_##name)` (public/scoped).
- `B(name)` — same plus added to `builtin_functions` (frontend-implemented
  builtin functions).

Examples:
- `S(__iter__)`, `S(__next__)`, `S(__getitem__)`, `S(__setitem__)`,
  `S(__init__)`, `S(__call__)`, `S(__pyc_setslice__)`,
  `S(__pyc_getslice__)`, `S(write)`, `S(writeln)`.
- `P(__pyc_more__)`, `P(__pyc_to_bool__)`, `P(__pyc_to_str__)`,
  `P(__pyc_format_string__)`.
- `B(print)`, `B(super)`, `B(__pyc_c_call__)`, `B(__pyc_c_code__)`,
  `B(__pyc_insert_c_header__)`, `B(__pyc_insert_c_code__)`,
  `B(__pyc_include_c_header__)`, `B(__pyc_symbol__)`,
  `B(__pyc_clone_constants__)`.

The three uses are at:
- `python_ifa_util.cc:20` — `S` defines `Sym *sym_##name = 0;`.
- `python_ifa_int.h:27` — `S` declares `extern Sym *sym_##name;`.
- `python_ifa_sym.cc:71,104` — `S` populates `sym_##_x = if1_make_symbol(...)`
  and `B` populates `builtin_functions`.
- `python_ifa_main.cc:29` — `P` scopes them in `build_environment`.

**To add a new builtin name:** add the appropriate macro line to
`pyc_symbols.h`, then handle it wherever needed (typically the
primitive registration in `add_primitive_transfer_functions` or the
`build_builtin_call_pyda` switch).

---

## 10. Primitives (frontend side)

The frontend registers transfer functions for compiler primitives in
`add_primitive_transfer_functions` (`python_ifa_main.cc:108`):

```c
prim_reg(sym_write->name,           return_nil_transfer_function,    write_codegen);
prim_reg(sym_writeln->name,         return_nil_transfer_function,    writeln_codegen);
prim_reg(sym___pyc_c_call__->name,  c_call_transfer_function,        c_call_codegen);
prim_reg(sym___pyc_format_string__->name, format_string_transfer_function, format_string_codegen);
prim_reg(sym___pyc_to_str__->name,  to_str_transfer_function,        to_str_codegen);
prim_reg("to_string",               return_string_transfer_function);
```

Each registers a transfer function (called by IFA during analysis) and
optionally a codegen function (called by `cg.cc` to emit the C). For
the full primitive model see `PRIMITIVES.md` (TODO).

---

## 11. Language extensions

pyc exposes a small set of compiler directives. The Python-source forms
(left) and what they desugar to:

| Source form | Effect |
|---|---|
| `from pyc_compat import __pyc_declare__` | No-op import (skipped by `build_import_syms`). |
| `__pyc_declare__(field, type)` | Declares a polymorphic field on a class. The marker Sym `sym_declare` has `is_fake = true`. |
| `__pyc_c_call__(ret_type, "fn_name", arg1, arg2, ...)` | Emits a raw C call. Transfer function `c_call_transfer_function` types the result. |
| `__pyc_c_code__(...)` / `__pyc_insert_c_code__(...)` / `__pyc_insert_c_header__(...)` / `__pyc_include_c_header__(...)` | Inject raw C code/headers. Accumulated in `ctx.c_code` and emitted via `c_codegen_pre_file`. |
| `__pyc_symbol__(name)` | Construct a user-level symbol Sym. |
| `__pyc_clone_constants__(arg)` | Mark `arg`'s Sym with `clone_for_constants = 1`, hinting IFA to specialise on constant values. |
| `__pyc_char__` | Alias for `sym_uint8`. |
| `__pyc_operator__` | Alias for `sym_operator` (raw IF1 send target). |
| `__pyc_primitive__` | Alias for `sym_primitive`. |
| `__pyc_to_bool__` / `__pyc_to_str__` / `__pyc_more__` / `__pyc_format_string__` | Magic method names invoked by the frontend during lowering of `if`/`str(...)`/iterator-protocol/f-strings. |
| `@vector("s")` decorator | In `gen_class_pyda`, transforms the class into a fixed-size value-type vector. `s` is the size element. |

To add a new directive: declare it in `pyc_symbols.h` (`B(...)`), then
either:
- Register a primitive transfer function in
  `add_primitive_transfer_functions`, OR
- Handle it as a special case in `build_builtin_call_pyda`
  (`python_ifa_build_if1.cc:246`).

---

## 12. The DParser grammar (`python.g`)

DParser is a scannerless GLR parser generator. Key facts:

- One grammar file `python.g`; compiled to `python.g.d_parser.cc`
  (13840 lines) by `make_dparser` at build time.
- Whitespace handler `python_whitespace` (in
  `python.g.d_parser.cc`) tracks INDENT/DEDENT and implicit line
  joining via `PythonGlobals::indent_stack` and `current_indent`.
- `PythonGlobals` extends DParser's user-globals; `root_ast` is set by
  the `file_input` action and read by `dparse_python_to_ast`.
- Grammar actions construct `PyDAST` nodes via helpers in `python.g`
  (inline C++).

`D_ParseNode_User` is overridden in `python_parse.h` to
`PyParseNode { PyDAST *ast; }` so each parse node carries an
`ast` field.

### 12.1 Building the grammar

The grammar uses EBNF `*` for binary operator chains (e.g., `arith_expr`,
`term`) with custom C++ iteration in the action to detect the operator
character via `iter->start_loc.s[0]`. This avoids left-recursive rules
that cause DParser GLR state-table blowup.

From project memory: regenerating `python.g.d_parser.cc` after a
grammar change takes time (~45K states) and the larger file slows
parsing of large inputs. Keep EBNF `*` form rather than left recursion
to manage state count.

### 12.2 Validation modes

- `pyc --dparse_only file.py` — parse only; success or syntax error.
  Useful for grammar regressions.
- `pyc --dparse_ast file.py` — parse + print AST. Useful for debugging
  what shape a grammar rule produces.

`make test_dparse` (in the Makefile) runs both over the test corpus.

---

## 13. Gotchas

### 13.1 The dual annotation paths
Both `PyDAST` and `PycAST` have IFA annotation fields (`code`, `sym`,
`rval`, etc.). The current pyda path stores on `PycAST` (via `getAST`);
the `PyDAST` fields are residue from an earlier path. Prefer `PycAST`
for new code.

### 13.2 `getAST` form choice
`getAST(n, ctx)` and `getAST(n, parent_ast)` differ only in where they
get `filename` and `is_builtin`. Pick the one whose source has the right
context. Using the wrong form usually shows up as wrong filenames in
diagnostics or as builtin-flagged nodes leaking into user output.

### 13.3 Scope sentinel arithmetic
The four sentinels are `(intptr_t)1..4` cast to `PycSymbol*`. Any
real `PycSymbol*` allocated by `new` will be way above this range, but
**don't dereference a value from `PycScope::map` without `MARKED(x)`
check first** or you'll segfault on the sentinel.

### 13.4 `Vec::move` for scope save/restore
When saving `scope_stack` across an import, use `Vec::move`, not the
`n = 0` trick. Project memory documents the bug; the test suite would
flag this but only on specific import patterns.

### 13.5 The global guard in `make_PycSymbol`
When `scope_stack.n == 1`, the global scope and the local scope are the
same. Writing a `GLOBAL_DEF` sentinel into "local" overwrites the
actual symbol. The check `!(ctx.scope_stack.n == 1)` is load-bearing.

### 13.6 `Partial_NEVER` is the pyc default
`ast_to_if1` sets `if1->partial_default = Partial_NEVER`
(`python_ifa_main.cc:208`). Every SEND defaults to "no partial
application." Frontends that want curry-style partial application must
set this back to `Partial_OK` per-SEND.

### 13.7 `test_scoping` exits before if1
`if (test_scoping) exit(0)` runs after `build_syms` but before
`build_if1`. The test compares stdout (which `TEST_SCOPE` writes to).
**stderr must be empty**: the test harness redirects both with `>&` and
fails on any stderr output. Don't add `fprintf(stderr, ...)` in code
paths that run during scoping tests — debug prints there will break
the test (this was a real fix in `gen_class_pyda`).

### 13.8 `__new__` wrapper default-arg discovery
`gen_class_pyda` synthesises a `__new__` wrapper Fun whose `ast` points
to the classdef node, not to `__init__`. So `finalize_function` checks
`fn->init` (which points to the actual `__init__` Sym) to find the
varargslist that holds the defaults. The `if (kind == PY_classdef ||
kind == PY_decorated)` branch in `finalize_function` is precisely this
case — don't shortcut it back to "return early."

### 13.9 `@vector("s")` decorator unwrapping
The DParser AST shape is:

```
PY_decorated
  ├─ PY_suite
  │   └─ PY_decorator
  │       ├─ PY_dotted_name('vector')
  │       └─ PY_arglist
  │           └─ PY_string('"s"')
  └─ PY_classdef
```

Decorators are wrapped in `PY_suite`, not direct children. Code that
walks decorators must unwrap the `PY_suite` first.

### 13.10 Builtin Sym redefinition
`build_builtin_symbols` (`python_ifa_sym.cc:70`) sets up name aliases:
- `sym_int->alias = sym_int64` (Python `int` ≡ `int64`).
- `sym_float->alias = sym_float64`.
- `sym_complex->alias = sym_complex64`.
- `sym_size->alias = sym_int64`.
- `sym_char->alias = sym_string`.
- `sym_string->name = "str"`.
- `sym_nil->name = "None"`.
- etc.

If you read a builtin Sym and the name looks "wrong," check this
function.

### 13.11 `pyc_compat.py` is a no-op shim
The import is recognised and skipped by `build_import_syms`. The
file exists only so `import` lines pyc programs use are runnable under
unmodified CPython. Adding fields to `pyc_compat` is fine; just don't
expect pyc to *do* anything with the imports.

### 13.12 Generated parser slowness on `__pyc__`
The DParser GLR engine is super-linear on large complex files. The
builtin module is intentionally loaded via `dparse_builtin_dir` (which
concatenates the small files) rather than as one giant file, partly for
parse speed. If you grow `__pyc__/*.py`, watch the parse time.

---

## 14. Where to read first when something's wrong

| Symptom | Start here |
|---|---|
| "name not defined" | `find_PycSymbol`, then `make_PycSymbol`, then the scope sentinel handling |
| "wrong scope for variable" | The scope-stack mutations in `build_syms_pyda` for the relevant statement kind; check sentinels |
| "import not found" | `build_import_syms`, `import_file`, `ctx.search_path` from `build_search_path` |
| "default argument not applied" | `finalize_function` and `PycCompiler::default_wrapper` |
| "missing instance variable / no-type violation" | `PycCompiler::reanalyze` — adds ivar Syms on the fly |
| "wrong primitive transfer function" | `add_primitive_transfer_functions` and `prim_reg` calls |
| "wrong call dispatch on method" | `gen_class_pyda` + class inheritance + IFA pattern matching (`DISPATCH.md` when written) |
| "decorator not applied" | `PY_decorated` handling in both passes; check `PY_suite` unwrapping |
| "syntax error" | `python.g`, `python_whitespace`, or run with `--dparse_only` |
| "scoping test failure" | Check for stray stderr output; rerun with `-v -d --test_scoping` |
| "`__pyc__` builtin missing" | `IFA_SYSTEM_DIRECTORY` env var; `dparse_builtin_dir` succeeded? |

---

## 15. References

- `pyc.cc` — entry point.
- `python_parse.{cc,h}` + `python.g` — parser.
- `python_ast.h` — PyDAST.
- `python_ifa_*.{cc,h}` — symbol/IF1 passes.
- `python_ifa.h` — public callback / wrapper class headers.
- Sister docs:
  [PIPELINE.md](PIPELINE.md) (overall flow),
  [ifa/ARCHITECTURE.md](ifa/ARCHITECTURE.md) (IFA-side architecture),
  [ifa/IR.md](ifa/IR.md) (the IR this frontend produces),
  [ifa/IFA.md](ifa/IFA.md) (what happens to the IR next).
- `ROADMAP.md` — Python feature status (what's parsed, what's compiled).
- Project memory `MEMORY.md` — historical fixes (the OOP conversion,
  scope guard bug, scoping-test stderr fix, etc.) worth re-reading
  before invasive refactors.
