// SPDX-License-Identifier: BSD-3-Clause
// A static subset of CPython's `ctypes`, lowered before build_syms.
//
// A program that declares its foreign functions the way ctypes asks --
//
//     lib = ctypes.CDLL("libm.so.6")
//     lib.cbrt.argtypes = [ctypes.c_double]
//     lib.cbrt.restype = ctypes.c_double
//
// -- has said everything a static compiler needs: the library, the symbol
// and its C signature. This pass reads those declarations at compile time
// and rewrites them into ordinary pyc Python plus a C shim:
//
//  - `lib = CDLL(...)` becomes an instance of a generated class with one
//    METHOD per declared function, so `lib.f(x)` is plain method dispatch
//    from any module and `g = lib.f` is a plain bound method;
//  - `class S(Structure)` becomes a class over C-layout storage, with a
//    @property per field, and `POINTER(S)` a generated pointer class;
//  - each method converts its arguments by dispatch (`a.__pyc_ffi_c_int__()`,
//    one method per canonical ctypes class), so a wrong argument type is an
//    ordinary compile-time type error, and calls `_CG_ffi_<lib>_<f>` through
//    __pyc_c_call__;
//  - `c_int(5)` is a box over a one-scalar C cell; boxes and the pointers to
//    scalars are generated into the builtin module, shared by every module;
//  - the shim (pyc_ffi_shim) defines `_CG_ffi_<lib>_<f>` over machine words
//    and doubles only, and does the narrowing to the declared C types
//    itself, so the real C compiler owns the ABI on both backends. The
//    foreign symbol is declared under a private name bound with an asm
//    label, so it can never clash with a header's own prototype.
//
// The same source runs under CPython, which is the point: the declarations
// are CPython's, not a pyc dialect.
//
// What is NOT here is anything ctypes resolves at run time: a library or a
// signature that is not a literal at module top level is refused, never
// guessed. See issues/188.
#include "python_ifa_int.h"
#include "python_parse.h"
#include <map>
#include <string>
#include <vector>

namespace {

enum Kind { K_VOID, K_INT, K_BOOL, K_FLOAT, K_CHAR, K_CHARP, K_VOIDP, K_PTR, K_STRUCT, K_ARRAY };

struct FStruct;
// A ctypes type. Scalars are values; K_STRUCT names its class; K_PTR is a
// tree over its pointee, interned (intern_ptr) so that each distinct
// pointer type has one id and one generated class, as CPython caches
// POINTER(T) per T.
struct FType {
  Kind kind = K_INT;
  std::string c = "int";          // the C spelling, for the scalar kinds
  FStruct *s = nullptr;           // K_STRUCT
  const FType *elem = nullptr;    // K_PTR: the pointee; K_ARRAY: the element (interned)
  int id = -1;                    // K_PTR, K_ARRAY: the interned type's id (one space each)
  int n = 0;                      // K_ARRAY: the length
  cchar *canon = nullptr;         // a scalar: its canonical ctypes class (c_int32 is c_int)
};
struct FField {
  std::string name;
  FType t;
};
struct FStruct {
  std::string name;
  int id = 0;
  bool has_fields = false;
  bool user_init = false;
  std::vector<FField> fields;
  PyDAST *cdef = nullptr;
  const FType *ptr = nullptr;  // POINTER(this), interned when the class is read
};
struct FFun {
  std::string name;
  bool has_args = false, has_res = false;
  std::vector<FType> args;
  FType res;  // ctypes' default restype is c_int
  cchar *filename = nullptr;
  int line = 0;
};
struct FLib {
  std::string var;
  std::string file;  // "" for CDLL(None)
  int id = 0;
  std::vector<FFun *> funs;
  PyDAST *stmt = nullptr;
};
struct FunRef {
  FLib *lib;
  FFun *fun;
};
struct Env {
  cchar *filename = nullptr;
  std::vector<FType *> ptr_order;            // pointer types first named in this module
  std::vector<FType *> arr_order;            // array types first named in this module
  std::map<std::string, long> int_consts;    // NAME = <int literal>, for an array length
  std::vector<std::string> mod_names;        // names bound to the ctypes module
  std::map<std::string, std::string> from;   // local name -> dotted ctypes name
  bool star = false;                         // from ctypes import *
  std::map<std::string, FStruct *> structs;  // by class name
  std::map<std::string, FLib *> libs;        // by variable name
  std::map<std::string, FType> type_alias;   // NAME = c_int / POINTER(S)
  std::map<std::string, FunRef> fun_alias;   // NAME = lib.f
  std::vector<FStruct *> struct_order;
  std::vector<FLib *> lib_order;
};

struct Scalar {
  cchar *name;
  Kind kind;
  cchar *c;
  cchar *canon;  // the class this name is bound to in CPython on an LP64 target
};
const Scalar scalars[] = {
    {"c_bool", K_BOOL, "bool", "c_bool"},
    {"c_char", K_CHAR, "char", "c_char"},
    {"c_byte", K_INT, "signed char", "c_byte"},
    {"c_ubyte", K_INT, "unsigned char", "c_ubyte"},
    {"c_short", K_INT, "short", "c_short"},
    {"c_ushort", K_INT, "unsigned short", "c_ushort"},
    {"c_int", K_INT, "int", "c_int"},
    {"c_uint", K_INT, "unsigned int", "c_uint"},
    {"c_long", K_INT, "long", "c_long"},
    {"c_ulong", K_INT, "unsigned long", "c_ulong"},
    {"c_longlong", K_INT, "long long", "c_long"},
    {"c_ulonglong", K_INT, "unsigned long long", "c_ulong"},
    {"c_size_t", K_INT, "size_t", "c_ulong"},
    {"c_ssize_t", K_INT, "ptrdiff_t", "c_long"},
    {"c_int8", K_INT, "int8_t", "c_byte"},
    {"c_uint8", K_INT, "uint8_t", "c_ubyte"},
    {"c_int16", K_INT, "int16_t", "c_short"},
    {"c_uint16", K_INT, "uint16_t", "c_ushort"},
    {"c_int32", K_INT, "int32_t", "c_int"},
    {"c_uint32", K_INT, "uint32_t", "c_uint"},
    {"c_int64", K_INT, "int64_t", "c_long"},
    {"c_uint64", K_INT, "uint64_t", "c_ulong"},
    {"c_float", K_FLOAT, "float", "c_float"},
    {"c_double", K_FLOAT, "double", "c_double"},
    {"c_char_p", K_CHARP, "char *", "c_char_p"},
    {"c_void_p", K_VOIDP, "void *", "c_void_p"},
};
const Scalar *scalar_named(const std::string &s) {
  for (const Scalar &sc : scalars)
    if (s == sc.name) return &sc;
  return nullptr;
}
// The scalar type a ctypes name denotes, spelled as its canonical class.
FType scalar_type(const Scalar &sc) {
  const Scalar *c = scalar_named(sc.canon);
  FType t;
  t.kind = c->kind;
  t.c = c->c;
  t.canon = c->name;
  return t;
}
// The ctypes names that survive lowering as ordinary run-time Python
// (pyc_lib/ctypes). Every other ctypes name is compile-time only.
const char *runtime_names[] = {"byref", "pointer", "sizeof"};
// Compile-time names this pass understands, beyond the scalar table.
const char *static_names[] = {"CDLL", "PyDLL", "cdll", "cdll.LoadLibrary", "Structure", "POINTER",
                              "util", "util.find_library"};

int n_structs = 0, n_libs = 0, n_ptrs = 0;
// Every interned pointer type, by id, and by its structural key.
std::vector<FType *> ptr_types;
std::map<std::string, FType *> ptr_interned;
// A scalar box (`c_int(5)`, `p.contents` of a POINTER(c_int)): one per
// canonical scalar class, by id, and by canonical name. Its class and its
// accessors are generated into the builtin module, so every module shares
// them, as CPython has one c_int.
struct FBox {
  FType t;
  int id;
  const FType *ptr;  // POINTER(this scalar)
};
std::vector<FBox *> boxes;
std::map<std::string, FBox *> box_interned;
// Every interned array type (`c_int * 4`), by id, and by structural key:
// CPython caches these too, `(c_int * 4) is (c_int * 4)`.
int n_arrs = 0;
std::vector<FType *> arr_types;
std::map<std::string, FType *> arr_interned;
std::string shim;          // the C++ shim, accumulated over every module
bool builtin_needed = false;

[[noreturn]] void err(Env &env, PyDAST *n, cchar *fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  fail("error %s:%d: ctypes: %s", env.filename ? env.filename : "?", n ? n->line : 0, buf);
  exit(1);
}

std::string trimmed(cchar *s) {
  std::string r = s ? s : "";
  while (r.size() && (r.back() == ' ' || r.back() == '\t' || r.back() == '\n' || r.back() == '\r')) r.pop_back();
  return r;
}

std::string fmt(cchar *f, ...) {
  char buf[4096];
  va_list ap;
  va_start(ap, f);
  vsnprintf(buf, sizeof(buf), f, ap);
  va_end(ap);
  return buf;
}

bool has(std::vector<std::string> &v, const std::string &s) {
  for (auto &x : v)
    if (x == s) return true;
  return false;
}

bool is_scalar_name(const std::string &s) { return scalar_named(s) != nullptr; }
bool is_known_name(const std::string &s) {
  if (is_scalar_name(s)) return true;
  for (cchar *n : runtime_names)
    if (s == n) return true;
  for (cchar *n : static_names)
    if (s == n) return true;
  return false;
}
bool is_runtime_name(const std::string &s) {
  for (cchar *n : runtime_names)
    if (s == n) return true;
  return false;
}

// A plain string literal's contents, or false. Escapes are refused: the
// names this pass reads (libraries, fields) never need one.
bool string_literal(PyDAST *n, std::string &out) {
  if (!n || n->kind != PY_string || !n->str_val) return false;
  cchar *s = n->str_val;
  if (*s != '"' && *s != '\'') return false;
  char q = *s;
  int len = (int)strlen(s);
  if (len < 2 || s[len - 1] != q) return false;
  if (len >= 6 && s[1] == q && s[2] == q) return false;  // triple-quoted
  out.assign(s + 1, len - 2);
  return out.find('\\') == std::string::npos && out.find(q) == std::string::npos;
}

// A decimal or hex integer literal's value, or false.
bool int_literal(PyDAST *n, long &out) {
  if (!n || n->kind != PY_number || !n->str_val) return false;
  std::string t;
  for (cchar *p = n->str_val; *p; p++)
    if (*p != '_') t += *p;
  if (t.empty() || !isdigit((unsigned char)t[0])) return false;
  char *end = nullptr;
  out = strtol(t.c_str(), &end, t.size() > 1 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X') ? 16 : 10);
  return end && !*end;
}

// The dotted ctypes name an expression denotes ("c_int", "cdll.LoadLibrary",
// "." for the module itself), or "" if it is not a ctypes reference.
// `nchildren` limits a PY_power to its first trailers.
std::string ctypes_ref(Env &env, PyDAST *e, int nchildren = -1) {
  if (!e) return "";
  if (e->kind == PY_name && e->str_val) {
    std::string nm = e->str_val;
    auto it = env.from.find(nm);
    if (it != env.from.end()) return it->second;
    if (has(env.mod_names, nm)) return ".";
    // `from ctypes import *` binds ctypes' own names, not its submodule.
    if (env.star && is_known_name(nm) && nm.compare(0, 4, "util")) return nm;
    return "";
  }
  if (e->kind != PY_power || !e->children.n) return "";
  int n = nchildren < 0 ? e->children.n : nchildren;
  std::string r = ctypes_ref(env, e->children[0]);
  if (r == "") return "";
  for (int i = 1; i < n; i++) {
    PyDAST *t = e->children[i];
    if (t->kind != PY_attribute || !t->children.n || !t->children[0]->str_val) return "";
    r = (r == "." ? std::string() : r + ".") + t->children[0]->str_val;
  }
  return r;
}

// `HEAD(args)`: the arguments of a call whose callee is a ctypes name.
bool ctypes_call(Env &env, PyDAST *e, std::string &callee, Vec<PyDAST *> &args) {
  if (!e || e->kind != PY_power || e->children.n < 2) return false;
  PyDAST *call = e->children.last();
  if (call->kind != PY_call) return false;
  callee = ctypes_ref(env, e, e->children.n - 1);
  if (callee == "" || callee == ".") return false;
  args.clear();
  for (PyDAST *c : call->children) {
    if (c->kind == PY_arglist)
      for (PyDAST *a : c->children) args.add(a);
    else
      args.add(c);
  }
  return true;
}

FStruct *struct_named(Env &env, PyDAST *e) {
  if (!e || e->kind != PY_name || !e->str_val) return nullptr;
  auto it = env.structs.find(e->str_val);
  return it == env.structs.end() ? nullptr : it->second;
}

// A structural key for a type: structs are nominal, pointers interned.
std::string type_key(const FType &t) {
  switch (t.kind) {
    case K_STRUCT: return fmt("s%d", t.s->id);
    case K_PTR: return fmt("p%d", t.id);
    case K_ARRAY: return fmt("a%d", t.id);
    default: return t.canon ? t.canon : t.c;
  }
}

void shim_header();
std::string c_to_word(const FType &t, const std::string &v);
std::string c_from_word(const FType &t, const std::string &v);
cchar *word_type(const FType &t);
FType ptr_to(Env &env, const FType &elem);

// The box of scalar `t`, with a generic read, write and sizeof in the shim.
FBox *box_of(Env &env, const FType &t) {
  FBox *&b = box_interned[t.canon];
  if (!b) {
    b = new FBox{t, (int)boxes.size(), nullptr};
    boxes.push_back(b);
    std::string c = t.c, lv = fmt("(*(%s *)(intptr_t)a)", c.c_str());
    shim_header();
    shim += fmt("_CG_FFI_DEF int64_t _CG_ffi_sizeof_s%d(void) { return (int64_t)sizeof(%s); }\n", b->id, c.c_str());
    shim += fmt("_CG_FFI_DEF %s _CG_ffi_rd_s%d(int64_t a) { return %s; }\n", word_type(t), b->id,
                c_to_word(t, lv).c_str());
    shim += fmt("_CG_FFI_DEF int64_t _CG_ffi_wr_s%d(int64_t a, %s v) { %s = %s; return 0; }\n", b->id, word_type(t),
                lv.c_str(), c_from_word(t, "v").c_str());
    b->ptr = ptr_types[ptr_to(env, t).id];
  }
  return b;
}
bool is_box_kind(Kind k) { return k != K_VOID && k != K_PTR && k != K_STRUCT && k != K_ARRAY; }
std::string c_decl(const FType &t, const std::string &d);

// elem * n: one interned type, and one generated class, per (element, length).
FType arr_to(Env &env, const FType &elem, long n) {
  FType *&a = arr_interned[fmt("A(%s)*%ld", type_key(elem).c_str(), n)];
  if (!a) {
    a = new FType;
    a->kind = K_ARRAY;
    a->c = "";
    a->elem = elem.kind == K_PTR ? ptr_types[elem.id] : elem.kind == K_ARRAY ? arr_types[elem.id] : new FType(elem);
    a->n = (int)n;
    a->id = n_arrs++;
    arr_types.push_back(a);
    env.arr_order.push_back(a);
    if (is_box_kind(elem.kind)) box_of(env, elem);  // the element accessors
    ptr_to(env, elem);  // an array decays to POINTER(elem)
  }
  return *a;
}

// POINTER(elem): one interned type, and one generated class, per pointee.
FType ptr_to(Env &env, const FType &elem) {
  FType *&p = ptr_interned[type_key(elem)];
  if (!p) {
    p = new FType;
    p->kind = K_PTR;
    p->c = "";
    p->elem = elem.kind == K_PTR ? ptr_types[elem.id] : new FType(elem);
    p->id = n_ptrs++;
    ptr_types.push_back(p);
    env.ptr_order.push_back(p);
    // p.contents of a POINTER(c_int) is a c_int box.
    if (is_box_kind(elem.kind)) box_of(env, elem);
  }
  return *p;
}

// The Structure a pointer type ultimately points to.
FStruct *ptr_root(const FType *t) {
  while (t && (t->kind == K_PTR || t->kind == K_ARRAY)) t = t->elem;
  return t && t->kind == K_STRUCT ? t->s : nullptr;
}

bool eval_type(Env &env, PyDAST *e, FType &out);
void voidp_result(Env &env, PyDAST *at, cchar *what);

// POINTER(arg), at `at`.
FType pointer_type(Env &env, PyDAST *at, PyDAST *arg) {
  FType el;
  if (!eval_type(env, arg, el) || el.kind == K_VOID)
    err(env, at,
        "POINTER(...) of anything but a ctypes scalar, a Structure subclass defined above in this module, or a "
        "pointer to one, is not supported (for an untyped address use c_void_p)");
  if (el.kind == K_VOIDP) voidp_result(env, at, "pointee");
  if (el.kind == K_ARRAY) err(env, at, "POINTER of an array type is not supported; use POINTER of its element");
  return ptr_to(env, el);
}

// A ctypes type expression. `what` names the context for the diagnostic.
bool eval_type(Env &env, PyDAST *e, FType &out) {
  if (!e) return false;
  if (e->kind == PY_name && e->str_val) {
    if (!strcmp(e->str_val, "None")) {
      out = FType{K_VOID, "void", nullptr};
      return true;
    }
    auto ta = env.type_alias.find(e->str_val);
    if (ta != env.type_alias.end()) {
      out = ta->second;
      return true;
    }
    if (FStruct *s = struct_named(env, e)) {
      out = FType{K_STRUCT, "", s};
      return true;
    }
  }
  if (e->kind == PY_binop && e->op == PY_OP_MUL && e->children.n == 2) {
    FType el;
    if (!eval_type(env, e->children[0], el)) return false;
    PyDAST *ne = e->children[1];
    long n = -1;
    if (int_literal(ne, n)) {
    } else if (ne->kind == PY_name && ne->str_val && env.int_consts.count(ne->str_val)) n = env.int_consts[ne->str_val];
    if (n < 0) err(env, e, "an array length must be a non-negative integer literal, or a module-level name bound to one");
    if (el.kind == K_VOID) err(env, e, "an array of None is not a type");
    if (el.kind == K_STRUCT && !el.s->has_fields) err(env, e, "an array of %s before its _fields_ are set", el.s->name.c_str());
    if (el.kind == K_VOIDP) voidp_result(env, e, "array element");
    out = arr_to(env, el, n);
    return true;
  }
  std::string callee;
  Vec<PyDAST *> args;
  if (ctypes_call(env, e, callee, args)) {
    if (callee != "POINTER") return false;
    if (args.n != 1) err(env, e, "POINTER takes one type");
    out = pointer_type(env, e, args[0]);
    return true;
  }
  std::string r = ctypes_ref(env, e);
  if (r == "") return false;
  if (const Scalar *sc = scalar_named(r)) {
    out = scalar_type(*sc);
    return true;
  }
  err(env, e, "the type ctypes.%s is not supported", r.c_str());
}

void need_type(Env &env, PyDAST *e, FType &out, cchar *what) {
  if (!eval_type(env, e, out)) err(env, e, "%s must be a ctypes type written out literally", what);
}

// A list or tuple display's elements; a parenthesised single item is itself.
bool sequence_items(PyDAST *e, Vec<PyDAST *> &out) {
  if (!e || (e->kind != PY_list && e->kind != PY_tuple)) return false;
  for (PyDAST *c : e->children) out.add(c);
  return true;
}

// c_void_p as a RESULT is `int` or `None` in CPython, a union with no
// static representation. The accommodation is the address as an int, with
// NULL as 0; --strict refuses it. POINTER(S) is the CPython-identical way
// to carry an address.
void voidp_result(Env &env, PyDAST *at, cchar *what) {
  static int allow = -1;
  if (allow < 0) allow = getenv("PYC_FFI_VOIDP_INT") ? atoi(getenv("PYC_FFI_VOIDP_INT")) : 1;
  if (!runtime_errors || !allow)
    err(env, at,
        "%s of type c_void_p has no static representation: CPython yields an int, or None for NULL. "
        "Use POINTER(<Structure subclass>), or permissive mode's PYC_FFI_VOIDP_INT accommodation",
        what);
  fprintf(stderr,
          "warning: %s:%d: PYC_FFI_VOIDP_INT: a c_void_p %s is an int, and NULL is 0 where CPython gives None "
          "(a CPython deviation; use POINTER(...) to avoid it)\n",
          env.filename, at ? at->line : 0, what);
}

std::string c_type(const FType &t) {
  switch (t.kind) {
    case K_VOID: return "void";
    case K_CHARP: return "char *";
    case K_VOIDP: return "void *";
    case K_PTR: {
      if (t.elem->kind == K_ARRAY) return "void *";  // only the decay of an array of arrays
      std::string e = c_type(*t.elem);
      return e + (e.back() == '*' ? "*" : " *");
    }
    case K_STRUCT: return fmt("struct _CG_ffi_s_%d", t.s->id);
    case K_ARRAY: fail("internal error: ctypes: an array type has no C spelling without a declarator"); return "";
    default: return t.c;
  }
}

// A C declaration of `d` with type `t`: `int v[3]`, `struct _CG_ffi_s_1 s`.
// With d empty, the type name (`int [3]`), for sizeof.
std::string c_decl(const FType &t, const std::string &d) {
  if (t.kind == K_ARRAY) return c_decl(*t.elem, fmt("%s[%d]", d.c_str(), t.n));
  return c_type(t) + " " + d;
}

// How the shim turns a pyc-side word `v` into the C value, and back.
std::string c_from_word(const FType &t, const std::string &v) {
  switch (t.kind) {
    case K_FLOAT: return fmt("(%s)%s", t.c.c_str(), v.c_str());
    case K_STRUCT: return fmt("*(%s *)(intptr_t)%s", c_type(t).c_str(), v.c_str());
    case K_CHARP: case K_VOIDP: case K_PTR: return fmt("(%s)(intptr_t)%s", c_type(t).c_str(), v.c_str());
    default: return fmt("(%s)%s", t.c.c_str(), v.c_str());
  }
}
std::string c_to_word(const FType &t, const std::string &v) {
  switch (t.kind) {
    case K_FLOAT: return fmt("(double)(%s)", v.c_str());
    case K_CHARP: case K_VOIDP: case K_PTR: return fmt("(int64_t)(intptr_t)(%s)", v.c_str());
    default: return fmt("(int64_t)(%s)", v.c_str());
  }
}
cchar *word_type(const FType &t) { return t.kind == K_FLOAT ? "double" : "int64_t"; }

// The Python expression converting argument `a` to the word the shim takes,
// with the __pyc_c_call__ type it is declared as. Each conversion is a
// method only the acceptable types define, so a wrong argument is a
// compile-time error where CPython raises ctypes.ArgumentError.
std::string py_arg(const FType &t, const std::string &a, cchar *&decl) {
  decl = "int";
  switch (t.kind) {
    // Per canonical class: a c_int box converts where a c_int is declared,
    // a c_long box does not, as in CPython.
    case K_INT: case K_BOOL: case K_CHAR: return fmt("%s.__pyc_ffi_%s__()", a.c_str(), t.canon);
    case K_FLOAT: decl = "float"; return fmt("%s.__pyc_ffi_%s__()", a.c_str(), t.canon);
    case K_CHARP: return "(0 if " + a + " is None else " + a + ".__pyc_ffi_charp__())";
    case K_VOIDP: return "(0 if " + a + " is None else " + a + ".__pyc_ffi_addr__())";
    case K_PTR: return fmt("(0 if %s is None else %s.__pyc_ffi_as_%d__())", a.c_str(), a.c_str(), t.id);
    case K_STRUCT: return fmt("%s.__pyc_ffi_val_%d__()", a.c_str(), t.s->id);
    default: fail("internal error: ctypes: no argument conversion for kind %d", (int)t.kind); return "";
  }
}

// The statements returning the Python value of the word-valued expression
// `call`, each line indented by `ind`.
std::string py_ret(const FType &t, const std::string &call, cchar *ind) {
  std::string i = ind;
  switch (t.kind) {
    case K_VOID: return i + call + "\n";
    case K_BOOL: return i + "return " + call + " != 0\n";
    case K_CHARP:
      return i + "__pyc_p__ = " + call + "\n" + i + "if __pyc_p__ == 0: return None\n" + i +
             "return __pyc_c_call__(bytes, \"_CG_ffi_cstr\", int, __pyc_p__)\n";
    case K_PTR: return i + fmt("return __pyc_ffi_mk_%d__(%s)\n", t.id, call.c_str());
    case K_CHAR: return i + "return __pyc_c_call__(bytes, \"_CG_ffi_char\", int, " + call + ")\n";
    default: return i + "return " + call + "\n";
  }
}

void shim_header() {
  if (shim.size()) return;
  shim =
      "// pyc foreign-call shims, generated from the program's ctypes declarations.\n"
      "#include <stdint.h>\n"
      "#include <stddef.h>\n"
      "#ifndef _CG_FFI_DEF\n"
      "#define _CG_FFI_DEF static inline\n"
      "#endif\n"
      "#define _CG_FFI_STR2(x) #x\n"
      "#define _CG_FFI_STR(x) _CG_FFI_STR2(x)\n"
      "#define _CG_FFI_SYM(name) __asm__(_CG_FFI_STR(__USER_LABEL_PREFIX__) name)\n";
}

// ---------------------------------------------------------------- scanning

void scan_import(Env &env, PyDAST *n) {
  if (n->kind == PY_import_name) {
    Vec<PyDAST *> items;
    for (PyDAST *c : n->children) {
      if (c->kind == PY_testlist)
        for (PyDAST *d : c->children) items.add(d);
      else
        items.add(c);
    }
    for (PyDAST *d : items) {
      if (d->kind != PY_dotted_as_name || !d->children.n) continue;
      std::string mod = trimmed(d->children[0]->str_val);
      if (mod != "ctypes" && mod != "ctypes.util") continue;
      builtin_needed = true;
      if (d->children.n > 1 && d->children[1]->str_val) {
        if (mod == "ctypes")
          env.mod_names.push_back(d->children[1]->str_val);
        else
          env.from[d->children[1]->str_val] = "util";
      } else if (!has(env.mod_names, "ctypes"))
        env.mod_names.push_back("ctypes");
    }
    return;
  }
  if (n->kind != PY_import_from || !n->children.n) return;
  std::string mod = trimmed(n->children[0]->str_val);
  if (mod != "ctypes" && mod != "ctypes.util") return;
  builtin_needed = true;
  std::string prefix = mod == "ctypes" ? "" : "util.";
  if (n->children.n < 2) {
    if (mod == "ctypes") env.star = true;
    else env.from["find_library"] = "util.find_library";
    return;
  }
  // One name is the PY_import_as_name itself, several a list of them.
  Vec<PyDAST *> names;
  if (n->children[1]->kind == PY_import_as_name)
    names.add(n->children[1]);
  else
    for (PyDAST *c : n->children[1]->children) names.add(c);
  for (PyDAST *c : names) {
    if (c->kind != PY_import_as_name || !c->children.n || !c->children[0]->str_val) continue;
    std::string name = c->children[0]->str_val;
    std::string local = c->children.n > 1 && c->children[1]->str_val ? c->children[1]->str_val : name;
    env.from[local] = prefix + name;
  }
}

void body_statements(PyDAST *cdef, Vec<PyDAST *> &out) {
  PyDAST *body = cdef->children.last();
  if (body->kind == PY_suite)
    for (PyDAST *c : body->children) out.add(c);
  else
    out.add(body);
}

void set_fields(Env &env, FStruct *s, PyDAST *value) {
  if (s->has_fields) err(env, value, "the _fields_ of %s are set twice", s->name.c_str());
  Vec<PyDAST *> items;
  if (!sequence_items(value, items)) err(env, value, "_fields_ must be a literal list of (name, type) pairs");
  for (PyDAST *it : items) {
    Vec<PyDAST *> pair;
    FField f;
    if (!sequence_items(it, pair) || pair.n != 2 || !string_literal(pair[0], f.name))
      err(env, it, "a _fields_ entry must be a (\"name\", type) pair (bit fields are not supported)");
    need_type(env, pair[1], f.t, "a field type");
    if (f.t.kind == K_VOID) err(env, it, "a field cannot have type None");
    if (f.t.kind == K_STRUCT && !f.t.s->has_fields)
      err(env, it, "a field of type %s before its _fields_ are set", f.t.s->name.c_str());
    if (f.t.kind == K_VOIDP) voidp_result(env, it, "field");
    s->fields.push_back(f);
  }
  s->has_fields = true;
}

void scan_class(Env &env, PyDAST *n) {
  // children: name, bases..., body
  if (n->children.n != 3 || !n->children[0]->str_val) return;
  if (ctypes_ref(env, n->children[1]) != "Structure") return;
  FStruct *s = new FStruct;
  s->name = n->children[0]->str_val;
  s->id = n_structs++;
  s->cdef = n;
  env.structs[s->name] = s;
  env.struct_order.push_back(s);
  // byref(s) and pointer(s) are POINTER(S) objects, so every Structure has
  // its pointer type.
  s->ptr = ptr_types[ptr_to(env, FType{K_STRUCT, "", s}).id];
  shim_header();
  shim += fmt("struct _CG_ffi_s_%d;\n", s->id);
  Vec<PyDAST *> stmts;
  body_statements(n, stmts);
  for (PyDAST *st : stmts) {
    if (st->kind == PY_assign && st->children.n == 2 && st->children[0]->kind == PY_name &&
        st->children[0]->str_val) {
      cchar *t = st->children[0]->str_val;
      if (!strcmp(t, "_fields_")) {
        set_fields(env, s, st->children[1]);
        st->kind = PY_pass_stmt;
        st->children.clear();
      } else if (!strcmp(t, "_pack_") || !strcmp(t, "_anonymous_") || !strcmp(t, "_align_"))
        err(env, st, "%s is not supported", t);
    }
    PyDAST *def = st->kind == PY_decorated ? st->children.last() : st;
    if (def->kind == PY_funcdef && def->children.n && def->children[0]->str_val &&
        !strcmp(def->children[0]->str_val, "__init__"))
      err(env, def, "a Structure subclass defining __init__ is not supported");
  }
}

FFun *fun_of(FLib *lib, const std::string &name, cchar *filename, int line) {
  for (FFun *f : lib->funs)
    if (f->name == name) return f;
  FFun *f = new FFun;
  f->name = name;
  f->filename = filename;
  f->line = line;
  lib->funs.push_back(f);
  return f;
}

// `lib.f` or an alias of it, as a foreign-function reference.
bool fun_ref(Env &env, PyDAST *e, int nchildren, FunRef &out) {
  if (!e) return false;
  if (e->kind == PY_name && e->str_val) {
    auto it = env.fun_alias.find(e->str_val);
    if (it == env.fun_alias.end()) return false;
    out = it->second;
    return true;
  }
  if (e->kind != PY_power || !e->children.n) return false;
  int n = nchildren < 0 ? e->children.n : nchildren;
  PyDAST *h = e->children[0];
  if (h->kind != PY_name || !h->str_val) return false;
  if (n == 1) return fun_ref(env, h, -1, out);
  if (n != 2) return false;
  auto it = env.libs.find(h->str_val);
  PyDAST *a = e->children[1];
  if (it == env.libs.end() || a->kind != PY_attribute || !a->children.n || !a->children[0]->str_val) return false;
  out.lib = it->second;
  out.fun = fun_of(out.lib, a->children[0]->str_val, env.filename, e->line);
  return true;
}

void erase(PyDAST *st) {
  st->kind = PY_pass_stmt;
  st->children.clear();
  st->str_val = nullptr;
}

void scan_assign(Env &env, PyDAST *st) {
  if (st->children.n != 2) return;
  PyDAST *target = st->children[0], *value = st->children[1];
  // LIB.F.argtypes / LIB.F.restype / S._fields_
  if (target->kind == PY_power && target->children.n >= 2) {
    PyDAST *last = target->children.last();
    if (last->kind != PY_attribute || !last->children.n || !last->children[0]->str_val) return;
    cchar *attr = last->children[0]->str_val;
    if (!strcmp(attr, "_fields_") && target->children.n == 2) {
      if (FStruct *s = struct_named(env, target->children[0])) {
        set_fields(env, s, value);
        erase(st);
      }
      return;
    }
    bool is_args = !strcmp(attr, "argtypes"), is_res = !strcmp(attr, "restype");
    if (!is_args && !is_res) {
      if (!strcmp(attr, "errcheck")) {
        FunRef fr;
        if (fun_ref(env, target, target->children.n - 1, fr)) err(env, st, "errcheck is not supported");
      }
      return;
    }
    FunRef fr;
    if (!fun_ref(env, target, target->children.n - 1, fr)) return;
    FFun *f = fr.fun;
    if (is_args) {
      if (f->has_args) err(env, st, "the argtypes of %s are set twice", f->name.c_str());
      Vec<PyDAST *> items;
      if (!sequence_items(value, items)) err(env, st, "argtypes must be a literal list or tuple of ctypes types");
      for (PyDAST *it : items) {
        FType t;
        need_type(env, it, t, "an argtypes entry");
        if (t.kind == K_VOID) err(env, it, "an argument cannot have type None");
        if (t.kind == K_ARRAY)
          err(env, it, "an array argument type is not supported; declare POINTER(element), which an array converts to");
        f->args.push_back(t);
      }
      f->has_args = true;
    } else {
      if (f->has_res) err(env, st, "the restype of %s is set twice", f->name.c_str());
      need_type(env, value, f->res, "restype");
      if (f->res.kind == K_ARRAY) err(env, st, "a C function cannot return an array");
      if (f->res.kind == K_VOIDP) voidp_result(env, st, "result");
      f->has_res = true;
    }
    f->filename = env.filename;
    f->line = st->line;
    erase(st);
    return;
  }
  if (target->kind != PY_name || !target->str_val) return;
  std::string name = target->str_val;
  // NAME = CDLL("lib") / cdll.LoadLibrary("lib")
  std::string callee;
  Vec<PyDAST *> args;
  if (ctypes_call(env, value, callee, args)) {
    if (callee == "CDLL" || callee == "PyDLL" || callee == "cdll.LoadLibrary") {
      FLib *lib = new FLib;
      lib->var = name;
      lib->id = n_libs++;
      lib->stmt = st;
      if (args.n != 1) err(env, st, "%s takes exactly the library name here", callee.c_str());
      PyDAST *a = args[0];
      std::string inner;
      Vec<PyDAST *> iargs;
      std::string lit;
      if (a->kind == PY_name && a->str_val && !strcmp(a->str_val, "None")) {
        // the program itself and what it already links (libc, libm)
      } else if (string_literal(a, lit)) {
        lib->file = lit;
        if (lit.find('/') != std::string::npos)
          codegen_link_libs.add(dupstr(lit.c_str()));
        else {
#ifdef __APPLE__
          // libNAME.dylib / libNAME.N.dylib -> -lNAME
          std::string base = lit;
          size_t dot = base.find('.');
          if (base.compare(0, 3, "lib") == 0 && dot != std::string::npos)
            codegen_link_libs.add(dupstr(("-l" + base.substr(3, dot - 3)).c_str()));
          else
            codegen_link_libs.add(dupstr(lit.c_str()));
#else
          // The soname itself: `-l:libfoo.so.0` needs no -dev symlink.
          codegen_link_libs.add(dupstr(("-l:" + lit).c_str()));
#endif
        }
      } else if (ctypes_call(env, a, inner, iargs) && inner == "util.find_library" && iargs.n == 1 &&
                 string_literal(iargs[0], lit)) {
        lib->file = lit;
        codegen_link_libs.add(dupstr(("-l" + lit).c_str()));
      } else
        err(env, st,
            "the library must be a string literal, None, or find_library(\"name\"): pyc links it at compile time");
      env.libs[name] = lib;
      env.lib_order.push_back(lib);
      return;
    }
    if (callee == "POINTER") {
      FType t;
      need_type(env, value, t, "a type alias");
      env.type_alias[name] = t;
      erase(st);
      return;
    }
    return;
  }
  // NAME = 8  (an array length may name it)
  long lit;
  if (int_literal(value, lit)) {
    env.int_consts[name] = lit;
    return;
  }
  env.int_consts.erase(name);
  // NAME = c_int * 4  (an array type)
  if (value->kind == PY_binop && value->op == PY_OP_MUL) {
    FType t;
    if (eval_type(env, value, t)) {
      env.type_alias[name] = t;
      erase(st);
    }
    return;
  }
  // NAME = c_int  (a type alias)
  std::string r = ctypes_ref(env, value);
  if (r != "" && is_scalar_name(r)) {
    FType t;
    need_type(env, value, t, "a type alias");
    env.type_alias[name] = t;
    erase(st);
    return;
  }
  // NAME = OTHER_ALIAS
  if (value->kind == PY_name && value->str_val && env.type_alias.count(value->str_val)) {
    env.type_alias[name] = env.type_alias[value->str_val];
    erase(st);
    return;
  }
  // NAME = lib.f  (stays: a bound method of the generated class)
  FunRef fr;
  if (value->kind == PY_power && value->children.n == 2 && fun_ref(env, value, -1, fr)) env.fun_alias[name] = fr;
}

// A call's arguments (a PY_call trailer's children, flattened).
void call_args(PyDAST *call, Vec<PyDAST *> &args) {
  args.clear();
  for (PyDAST *c : call->children) {
    if (c->kind == PY_arglist)
      for (PyDAST *a : c->children) args.add(a);
    else
      args.add(c);
  }
}

PyDAST *parse_generated(Env &env, const std::string &text, PyDAST *at);

// Is `e` a ctypes type expression (a scalar, an alias, a Structure, POINTER(T))?
bool is_type_expr(Env &env, PyDAST *e) {
  if (!e) return false;
  if (e->kind == PY_name && e->str_val && (env.type_alias.count(e->str_val) || struct_named(env, e))) return true;
  std::string callee;
  Vec<PyDAST *> args;
  if (ctypes_call(env, e, callee, args)) return callee == "POINTER";
  if (e->kind == PY_binop && e->op == PY_OP_MUL && e->children.n == 2) return is_type_expr(env, e->children[0]);
  return is_scalar_name(ctypes_ref(env, e));
}

// Replace expression node `n` in place by the expression `text`.
void replace_expr(Env &env, PyDAST *n, const std::string &text) {
  PyDAST *g = parse_generated(env, "__pyc_x__ = " + text + "\n", n);
  PyDAST *v = g->children[0]->children[1];
  n->kind = v->kind;
  n->str_val = v->str_val;
  n->children.clear();
  for (PyDAST *c : v->children) n->add(c);
}

// Calls of a ctypes type, rewritten to the generated classes, as in CPython:
//  - `POINTER(T)()` and `ALIAS()` for a pointer type construct a NULL pointer;
//  - `c_int(5)`, `c_int()` and `ALIAS(5)` for a scalar construct a box;
//  - `sizeof(T)` of a type is the C compiler's answer (of an instance it is
//    the run-time `sizeof` in pyc_lib/ctypes).
// Runs after scanning, so every alias is known, and before generation, so the
// types it interns get their classes.
// Replace each PY_name `__pyc_ffi_argN__` under `n` by args[N].
void subst_args(PyDAST *n, Vec<PyDAST *> &args) {
  for (int i = 0; i < n->children.n; i++) {
    PyDAST *c = n->children[i];
    int k;
    if (c->kind == PY_name && c->str_val && sscanf(c->str_val, "__pyc_ffi_arg%d__", &k) == 1 && k < args.n) {
      args[k]->parent = n;
      n->children.v[i] = args[k];
    } else
      subst_args(c, args);
  }
}

// An array constructor `A(x, y)` at trailer `ctor` of power node `n`:
// `__pyc_ffi_A_K__().__pyc_ffi_ini__(0, x).__pyc_ffi_ini__(1, y)`, which
// stores each item as `a[i] = x` does and leaves the rest zero.
void rewrite_array_ctor(Env &env, PyDAST *n, int ctor, const FType &t) {
  Vec<PyDAST *> cargs;
  call_args(n->children[ctor], cargs);
  if (cargs.n > t.n) err(env, n, "too many initializers: an array of %d takes at most %d (CPython: IndexError)", t.n, t.n);
  for (PyDAST *a : cargs)
    if (a->kind == PY_keyword_arg || a->kind == PY_star_arg || a->kind == PY_dstar_arg)
      err(env, n, "an array is constructed from its items, positionally");
  std::string text = fmt("__pyc_ffi_A_%d__()", t.id);
  for (int i = 0; i < cargs.n; i++) text += fmt(".__pyc_ffi_ini__(%d, __pyc_ffi_arg%d__)", i, i);
  PyDAST *g = parse_generated(env, "__pyc_x__ = " + text + "\n", n);
  PyDAST *v = g->children[0]->children[1];
  subst_args(v, cargs);
  Vec<PyDAST *> rest;
  for (PyDAST *c : v->children) rest.add(c);
  for (int i = ctor + 1; i < n->children.n; i++) rest.add(n->children[i]);
  n->children.clear();
  for (PyDAST *c : rest) n->add(c);
}

void rewrite_ctors(Env &env, PyDAST *n) {
  if (!n) return;
  // `(c_int * 4)(1, 2)`
  if (n->kind == PY_power && n->children.n >= 2 && n->children[0]->kind == PY_binop && n->children[1]->kind == PY_call &&
      is_type_expr(env, n->children[0])) {
    FType t;
    need_type(env, n->children[0], t, "a constructed type");
    if (t.kind != K_ARRAY) err(env, n, "only an array type is constructed from an expression here");
    rewrite_array_ctor(env, n, 1, t);
  }
  if (n->kind == PY_power && n->children.n >= 2 && n->children[0]->kind == PY_name && n->children[0]->str_val) {
    PyDAST *h = n->children[0];
    int nref = 1;
    while (nref < n->children.n && n->children[nref]->kind == PY_attribute) nref++;
    std::string ref = ctypes_ref(env, n, nref);
    FType t;
    int ctor = -1;  // index of the constructing call trailer
    auto ta = env.type_alias.find(h->str_val);
    if (nref == 1 && ta != env.type_alias.end() && n->children[1]->kind == PY_call) {
      t = ta->second;
      ctor = 1;
    } else if (nref < n->children.n && n->children[nref]->kind == PY_call && is_scalar_name(ref)) {
      t = scalar_type(*scalar_named(ref));
      ctor = nref;
    } else if (nref + 1 < n->children.n && ref == "POINTER" && n->children[nref]->kind == PY_call &&
               n->children[nref + 1]->kind == PY_call) {
      Vec<PyDAST *> targs;
      call_args(n->children[nref], targs);
      if (targs.n != 1) err(env, n, "POINTER takes one type");
      t = pointer_type(env, n, targs[0]);
      ctor = nref + 1;
    } else if (nref + 1 == n->children.n && ref == "sizeof" && n->children[nref]->kind == PY_call) {
      Vec<PyDAST *> sargs;
      call_args(n->children[nref], sargs);
      if (sargs.n == 1 && is_type_expr(env, sargs[0])) {
        FType st;
        need_type(env, sargs[0], st, "sizeof's argument");
        std::string f;
        if (st.kind == K_PTR) f = "_CG_ffi_sizeof_ptr";
        else if (st.kind == K_ARRAY) f = fmt("_CG_ffi_sizeof_a%d", st.id);
        else if (st.kind == K_STRUCT) {
          if (!st.s->has_fields) err(env, n, "sizeof(%s) before its _fields_ are set", st.s->name.c_str());
          f = fmt("_CG_ffi_sizeof_%d", st.s->id);
        } else if (is_box_kind(st.kind))
          f = fmt("_CG_ffi_sizeof_s%d", box_of(env, st)->id);
        else
          err(env, n, "sizeof(None) is not a size");
        replace_expr(env, n, fmt("__pyc_c_call__(int, \"%s\")", f.c_str()));
        return;
      }
    }
    if (ctor >= 0 && t.kind == K_ARRAY) {
      rewrite_array_ctor(env, n, ctor, t);
      ctor = -1;
    }
    if (ctor >= 0) {
      Vec<PyDAST *> cargs;
      call_args(n->children[ctor], cargs);
      if (t.kind == K_PTR) {
        if (cargs.n) err(env, n, "constructing a pointer from a value is not supported; use pointer(x)");
        h->str_val = dupstr(fmt("__pyc_ffi_P_%d__", t.id).c_str());
      } else if (is_box_kind(t.kind)) {
        if (cargs.n > 1) err(env, n, "%s takes at most one value", t.canon);
        for (PyDAST *a : cargs)
          if (a->kind == PY_keyword_arg) err(env, n, "%s takes its value positionally here", t.canon);
        if (t.kind == K_VOIDP) voidp_result(env, n, "box's value");
        h->str_val = dupstr(fmt("__pyc_ffi_B_%d__", box_of(env, t)->id).c_str());
      } else
        ctor = -1;  // a Structure alias: an ordinary class call
      if (ctor >= 0) {
        // Drop the `ctypes.` / `POINTER(T)` trailers before the constructing call.
        Vec<PyDAST *> rest;
        rest.add(h);
        for (int i = ctor; i < n->children.n; i++) rest.add(n->children[i]);
        n->children.clear();
        for (PyDAST *c : rest) n->add(c);
      }
    }
  }
  for (PyDAST *c : n->children) rewrite_ctors(env, c);
}

// ------------------------------------------------------------- generation

void set_origin(PyDAST *n, cchar *filename, int line) {
  if (!n) return;
  n->filename = filename;
  n->line = line;
  n->col = 0;
  for (PyDAST *c : n->children) set_origin(c, filename, line);
}

PyDAST *parse_generated(Env &env, const std::string &text, PyDAST *at) {
  if (getenv("PYC_DBG_CTYPES")) fprintf(stderr, "--- ctypes: generated for %s:%d\n%s", env.filename, at->line, text.c_str());
  PyDAST *g = dparse_python_buf_to_ast("<ctypes>", text.c_str(), (int)text.size());
  if (!g) fail("internal error: ctypes lowering generated code that does not parse (%s:%d)", env.filename, at->line);
  set_origin(g, at->filename, at->line);
  return g;
}

void shim_struct(FStruct *s) {
  if (!s->has_fields) return;
  shim += fmt("struct _CG_ffi_s_%d {\n", s->id);
  for (FField &f : s->fields) shim += fmt("  %s;\n", c_decl(f.t, f.name).c_str());
  shim += "};\n";
  shim += fmt("_CG_FFI_DEF int64_t _CG_ffi_sizeof_%d(void) { return (int64_t)sizeof(struct _CG_ffi_s_%d); }\n", s->id,
              s->id);
  for (FField &f : s->fields) {
    // A field that is itself C storage (a Structure, an array) reads as a
    // view at its offset, and a pointer field as a view of its cell.
    if (f.t.kind == K_PTR || f.t.kind == K_STRUCT || f.t.kind == K_ARRAY)
      shim += fmt("_CG_FFI_DEF int64_t _CG_ffi_off_%d_%s(void) { return (int64_t)offsetof(struct _CG_ffi_s_%d, %s); }\n",
                  s->id, f.name.c_str(), s->id, f.name.c_str());
    if (f.t.kind == K_STRUCT || f.t.kind == K_ARRAY) continue;
    std::string lv = fmt("((struct _CG_ffi_s_%d *)(intptr_t)a)->%s", s->id, f.name.c_str());
    shim += fmt("_CG_FFI_DEF %s _CG_ffi_get_%d_%s(int64_t a) { return %s; }\n", word_type(f.t), s->id, f.name.c_str(),
                c_to_word(f.t, lv).c_str());
    shim += fmt("_CG_FFI_DEF int64_t _CG_ffi_set_%d_%s(int64_t a, %s v) { %s = %s; return 0; }\n", s->id,
                f.name.c_str(), word_type(f.t), lv.c_str(), c_from_word(f.t, "v").c_str());
  }
}

cchar *py_default(const FType &t) {
  switch (t.kind) {
    case K_FLOAT: return "0.0";
    case K_BOOL: return "False";
    case K_INT: case K_CHAR: return "0";
    default: return "None";
  }
}

// The size of one `t`, as a Python expression.
std::string size_expr(const FType &t) {
  switch (t.kind) {
    case K_PTR: return "__pyc_c_call__(int, \"_CG_ffi_sizeof_ptr\")";
    case K_STRUCT: return fmt("__pyc_c_call__(int, \"_CG_ffi_sizeof_%d\")", t.s->id);
    case K_ARRAY: return fmt("__pyc_c_call__(int, \"_CG_ffi_sizeof_a%d\")", t.id);
    default: return fmt("__pyc_c_call__(int, \"_CG_ffi_sizeof_s%d\")", box_interned[t.canon]->id);
  }
}
bool is_char_array(const FType &t) { return t.kind == K_ARRAY && t.elem->kind == K_CHAR; }

// The statements reading the C object of type `t` at address expression
// `at` as its Python value, `keep` keeping the storage alive: a scalar is
// its value, C storage a view over it.
std::string py_read_at(const FType &t, const std::string &at, const std::string &keep, cchar *ind) {
  switch (t.kind) {
    case K_PTR: return fmt("%sreturn __pyc_ffi_pview_%d__(%s, %s)\n", ind, t.id, at.c_str(), keep.c_str());
    case K_STRUCT: return fmt("%sreturn __pyc_ffi_view_%d__(%s, %s)\n", ind, t.s->id, at.c_str(), keep.c_str());
    case K_ARRAY:
      // CPython's special case: a c_char array field reads as bytes, to the first NUL.
      if (is_char_array(t))
        return fmt("%sreturn __pyc_c_call__(bytes, \"_CG_ffi_rd_cstr_n\", int, %s, int, %d)\n", ind, at.c_str(), t.n);
      return fmt("%sreturn __pyc_ffi_aview_%d__(%s, %s)\n", ind, t.id, at.c_str(), keep.c_str());
    default: {
      int b = box_interned[t.canon]->id;
      return py_ret(t, fmt("__pyc_c_call__(%s, \"_CG_ffi_rd_s%d\", int, %s)", t.kind == K_FLOAT ? "float" : "int", b, at.c_str()),
                    ind);
    }
  }
}

// The statements storing Python value `v` into the C object of type `t`
// at `at`: a scalar converts, C storage is copied (`t.s = S()` is a copy).
std::string py_write_at(const FType &t, const std::string &at, const std::string &v, cchar *ind) {
  std::string i = ind;
  switch (t.kind) {
    case K_PTR:
      return i + fmt("__pyc_c_call__(int, \"_CG_ffi_wr_ptr\", int, %s, int, (0 if %s is None else %s.__pyc_ffi_as_%d__()))\n",
                     at.c_str(), v.c_str(), v.c_str(), t.id);
    case K_STRUCT:
      return i + fmt("__pyc_c_call__(int, \"_CG_ffi_copy\", int, %s, int, %s.__pyc_ffi_val_%d__(), int, %s)\n", at.c_str(),
                     v.c_str(), t.s->id, size_expr(t).c_str());
    case K_ARRAY:
      if (is_char_array(t))
        return i + fmt("if len(%s) > %d: raise ValueError(\"bytes too long (\" + str(len(%s)) + \", maximum length %d)\")\n",
                       v.c_str(), t.n, v.c_str(), t.n) +
               i + fmt("__pyc_c_call__(int, \"_CG_ffi_wr_chars\", int, %s, bytes, %s, int, len(%s), int, %d)\n", at.c_str(),
                       v.c_str(), v.c_str(), t.n);
      return i + fmt("__pyc_c_call__(int, \"_CG_ffi_copy\", int, %s, int, %s.__pyc_ffi_aval_%d__(), int, %s)\n", at.c_str(),
                     v.c_str(), t.id, size_expr(t).c_str());
    default: {
      int b = box_interned[t.canon]->id;
      cchar *decl;
      std::string conv = py_arg(t, v, decl);
      return i + fmt("__pyc_c_call__(int, \"_CG_ffi_wr_s%d\", int, %s, %s, %s)\n", b, at.c_str(), decl, conv.c_str());
    }
  }
}
bool is_storage(const FType &t) { return t.kind == K_STRUCT || t.kind == K_ARRAY; }

std::string py_struct(FStruct *s) {
  std::string t = fmt("class %s(__pyc_ffi_struct__):\n", s->name.c_str());
  int k = s->id;
  if (s->has_fields) {
    t += "  def __init__(self";
    for (FField &f : s->fields) t += ", " + f.name + "=" + py_default(f.t);
    t += "):\n";
    t += fmt("    self._buf = __pyc_c_call__(bytes, \"_CG_ffi_buf\", int, __pyc_c_call__(int, \"_CG_ffi_sizeof_%d\"))\n", k);
    t += "    self._addr = __pyc_c_call__(int, \"_CG_ffi_bytes_addr\", bytes, self._buf)\n";
    for (FField &f : s->fields) {
      if (is_storage(f.t)) {  // zeroed unless given: the buffer starts zeroed
        t += fmt("    if %s is not None: self.__pyc_set_%s__(%s)\n", f.name.c_str(), f.name.c_str(), f.name.c_str());
        continue;
      }
      cchar *decl;
      std::string conv = py_arg(f.t, f.name, decl);
      t += fmt("    __pyc_c_call__(int, \"_CG_ffi_set_%d_%s\", int, self._addr, %s, %s)\n", k, f.name.c_str(), decl,
               conv.c_str());
    }
    for (FField &f : s->fields) {
      std::string at = fmt("self._addr + __pyc_c_call__(int, \"_CG_ffi_off_%d_%s\")", k, f.name.c_str());
      t += "  @property\n";
      t += fmt("  def %s(self):\n", f.name.c_str());
      if (f.t.kind == K_PTR || is_storage(f.t))
        t += py_read_at(f.t, at, "self._buf", "    ");
      else {
        std::string call = fmt("__pyc_c_call__(%s, \"_CG_ffi_get_%d_%s\", int, self._addr)",
                               f.t.kind == K_FLOAT ? "float" : "int", k, f.name.c_str());
        t += py_ret(f.t, call, "    ");
      }
      // The store side: `s.f = v` is `s.__pyc_set_f__(v)` (emit_member_store).
      t += fmt("  def __pyc_set_%s__(self, v):\n", f.name.c_str());
      if (is_storage(f.t))
        t += py_write_at(f.t, at, "v", "    ");
      else {
        cchar *decl;
        std::string conv = py_arg(f.t, "v", decl);
        t += fmt("    __pyc_c_call__(int, \"_CG_ffi_set_%d_%s\", int, self._addr, %s, %s)\n", k, f.name.c_str(), decl,
                 conv.c_str());
      }
      pyc_add_setter_name(f.name.c_str());
    }
    // byref(s) is CPython's opaque argument object: good as an argument and
    // nothing else. pointer(s) is the real POINTER(S) instance.
    t += "  def __pyc_ffi_byref__(self):\n";
    t += fmt("    return __pyc_ffi_R_%d__(self._addr, self._buf)\n", s->ptr->id);
    t += "  def __pyc_ffi_pointer__(self):\n";
    t += fmt("    return __pyc_ffi_mk_%d__(self._addr)\n", s->ptr->id);
    t += fmt("  def __pyc_ffi_val_%d__(self):\n", k);
    t += "    return self._addr\n";
    t += fmt("  def __pyc_ffi_target_%d__(self):\n", s->ptr->id);
    t += "    return self._addr\n";
    t += "  def __pyc_ffi_sizeof__(self):\n";
    t += fmt("    return __pyc_c_call__(int, \"_CG_ffi_sizeof_%d\")\n", k);
    // A view: an S over storage it does not own, keeping the owner alive.
    t += fmt("def __pyc_ffi_view_%d__(addr, keep):\n", k);
    t += fmt("  o = %s()\n", s->name.c_str());
    t += "  o._buf = keep\n";
    t += "  o._addr = addr\n";
    t += "  return o\n";
  } else
    t += "  pass\n";
  return t;
}

// POINTER(T). A pointer object is a one-word C cell holding the address,
// as in CPython: byref(p) is the cell's address, and since the cell lives
// in scanned storage it keeps a pyc-allocated pointee alive by itself.
std::string py_pointer(const FType *p) {
  int k = p->id;
  const FType *e = p->elem;
  std::string t = fmt("class __pyc_ffi_P_%d__(__pyc_ffi_pointer__):\n", k);
  t += fmt("  def __pyc_ffi_as_%d__(self):\n", k);
  t += "    return __pyc_c_call__(int, \"_CG_ffi_rd_ptr\", int, self._addr)\n";
  std::string view, size;
  if (e->kind == K_STRUCT && e->s->has_fields) {
    view = fmt("__pyc_ffi_view_%d__", e->s->id);
    size = fmt("__pyc_c_call__(int, \"_CG_ffi_sizeof_%d\")", e->s->id);
  } else if (e->kind == K_PTR) {
    view = fmt("__pyc_ffi_pview_%d__", e->id);
    size = "__pyc_c_call__(int, \"_CG_ffi_sizeof_ptr\")";
  } else if (e->kind == K_ARRAY) {
    view = fmt("__pyc_ffi_aview_%d__", e->id);
    size = size_expr(*e);
  } else if (is_box_kind(e->kind)) {
    int b = box_interned[e->canon]->id;
    view = fmt("__pyc_ffi_bview_%d__", b);
    size = fmt("__pyc_c_call__(int, \"_CG_ffi_sizeof_s%d\")", b);
  }
  std::string deref =
      "    __pyc_a__ = __pyc_c_call__(int, \"_CG_ffi_rd_ptr\", int, self._addr)\n"
      "    if __pyc_a__ == 0: raise ValueError(\"NULL pointer access\")\n";
  if (view.size()) {
    // The getter written out: this class may be generated into the builtin
    // module, which the @property scan does not visit.
    t += "  def __pyc_get_contents__(self):\n";
    pyc_add_property_name("contents");
    t += deref;
    t += fmt("    return %s(__pyc_a__, self._buf)\n", view.c_str());
    // p.contents = x points p at x; the cell is scanned, so it keeps x alive.
    t += "  def __pyc_set_contents__(self, v):\n";
    t += fmt("    __pyc_c_call__(int, \"_CG_ffi_wr_ptr\", int, self._addr, int, v.__pyc_ffi_target_%d__())\n", k);
    pyc_add_setter_name("contents");
    t += "  def __getitem__(self, i):\n";
    t += deref;
    if (is_box_kind(e->kind)) {
      // p[i] of a scalar pointee is the VALUE (type(p[0]) is int), and
      // p[i] = v stores it.
      int b = box_interned[e->canon]->id;
      std::string at = "__pyc_a__ + i * " + size;
      t += py_ret(*e, fmt("__pyc_c_call__(%s, \"_CG_ffi_rd_s%d\", int, %s)", e->kind == K_FLOAT ? "float" : "int", b,
                          at.c_str()),
                  "    ");
      cchar *decl;
      std::string conv = py_arg(*e, "v", decl);
      t += "  def __setitem__(self, i, v):\n";
      t += deref;
      t += fmt("    __pyc_c_call__(int, \"_CG_ffi_wr_s%d\", int, %s, %s, %s)\n", b, at.c_str(), decl, conv.c_str());
    } else
      t += fmt("    return %s(__pyc_a__ + i * %s, self._buf)\n", view.c_str(), size.c_str());
  }
  // byref(p) and pointer(p), when POINTER(POINTER(T)) is a declared type.
  auto pp = ptr_interned.find(type_key(*p));
  if (pp != ptr_interned.end()) {
    t += "  def __pyc_ffi_byref__(self):\n";
    t += fmt("    return __pyc_ffi_R_%d__(self._addr, self._buf)\n", pp->second->id);
    t += "  def __pyc_ffi_pointer__(self):\n";
    t += fmt("    return __pyc_ffi_mk_%d__(self._addr)\n", pp->second->id);
    t += fmt("  def __pyc_ffi_target_%d__(self):\n", pp->second->id);
    t += "    return self._addr\n";
  }
  // byref(x): CPython's opaque argument object, good as an argument only.
  t += fmt("class __pyc_ffi_R_%d__(__pyc_ffi_ref__):\n", k);
  t += fmt("  def __pyc_ffi_as_%d__(self):\n", k);
  t += "    return self._addr\n";
  // A new pointer holding address `a`: a foreign result, or pointer(x).
  t += fmt("def __pyc_ffi_mk_%d__(a):\n", k);
  t += fmt("  p = __pyc_ffi_P_%d__()\n", k);
  t += "  __pyc_c_call__(int, \"_CG_ffi_wr_ptr\", int, p._addr, int, a)\n";
  t += "  return p\n";
  // A pointer over a cell it does not own: a pointer field, or *pp.
  t += fmt("def __pyc_ffi_pview_%d__(addr, keep):\n", k);
  t += fmt("  p = __pyc_ffi_P_%d__()\n", k);
  t += "  p._buf = keep\n";
  t += "  p._addr = addr\n";
  t += "  return p\n";
  return t;
}

// c_int and its kin: a scalar in a one-value C cell, as a Structure is a
// cell of fields. Generated into the builtin module.
std::string py_box(FBox *b) {
  const FType &e = b->t;
  int k = b->id, pid = b->ptr->id;
  cchar *w = e.kind == K_FLOAT ? "float" : "int";
  std::string rd = fmt("__pyc_c_call__(%s, \"_CG_ffi_rd_s%d\", int, self._addr)", w, k);
  cchar *decl;
  std::string t = fmt("class __pyc_ffi_B_%d__(object):\n", k);
  t += fmt("  def __init__(self, value=%s):\n", py_default(e));
  t += fmt("    self._buf = __pyc_c_call__(bytes, \"_CG_ffi_buf\", int, __pyc_c_call__(int, \"_CG_ffi_sizeof_s%d\"))\n", k);
  t += "    self._addr = __pyc_c_call__(int, \"_CG_ffi_bytes_addr\", bytes, self._buf)\n";
  std::string conv = py_arg(e, "value", decl);
  t += fmt("    __pyc_c_call__(int, \"_CG_ffi_wr_s%d\", int, self._addr, %s, %s)\n", k, decl, conv.c_str());
  t += "  def __pyc_get_value__(self):\n";
  pyc_add_property_name("value");
  t += py_ret(e, rd, "    ");
  conv = py_arg(e, "v", decl);
  t += "  def __pyc_set_value__(self, v):\n";
  t += fmt("    __pyc_c_call__(int, \"_CG_ffi_wr_s%d\", int, self._addr, %s, %s)\n", k, decl, conv.c_str());
  pyc_add_setter_name("value");
  t += "  def __pyc_ffi_byref__(self):\n";
  t += fmt("    return __pyc_ffi_R_%d__(self._addr, self._buf)\n", pid);
  t += "  def __pyc_ffi_pointer__(self):\n";
  t += fmt("    return __pyc_ffi_mk_%d__(self._addr)\n", pid);
  t += fmt("  def __pyc_ffi_target_%d__(self):\n", pid);
  t += "    return self._addr\n";
  t += "  def __pyc_ffi_sizeof__(self):\n";
  t += fmt("    return __pyc_c_call__(int, \"_CG_ffi_sizeof_s%d\")\n", k);
  // Passed by value where its own class is declared.
  switch (e.kind) {
    case K_CHARP: t += "  def __pyc_ffi_charp__(self):\n"; break;
    case K_VOIDP: t += "  def __pyc_ffi_addr__(self):\n"; break;
    default: t += fmt("  def __pyc_ffi_%s__(self):\n", e.canon); break;
  }
  t += "    return " + rd + "\n";
  if (e.kind != K_CHARP && e.kind != K_VOIDP) {
    // CPython shows the canonical class: repr(c_int32(5)) is 'c_int(5)'.
    t += "  def __repr__(self):\n";
    t += fmt("    return \"%s(\" + repr(self.value) + \")\"\n", e.canon);
    t += "  def __str__(self):\n";
    t += "    return self.__repr__()\n";
  }
  t += fmt("def __pyc_ffi_bview_%d__(addr, keep):\n", k);
  t += fmt("  o = __pyc_ffi_B_%d__()\n", k);
  t += "  o._buf = keep\n";
  t += "  o._addr = addr\n";
  t += "  return o\n";
  return t;
}

// elem * n: C storage of n elements. `a[i]` is a value for a scalar
// element and a view for C storage, as in CPython.
std::string py_array(const FType *a) {
  int k = a->id, n = a->n;
  const FType &e = *a->elem;
  int pid = ptr_interned[type_key(e)]->id;  // POINTER(elem), which the array converts to
  std::string at = "self._addr + __pyc_i__ * " + size_expr(e);
  std::string check = fmt(
      "    __pyc_i__ = i + %d if i < 0 else i\n"
      "    if __pyc_i__ < 0 or __pyc_i__ >= %d: raise IndexError(\"invalid index\")\n",
      n, n);
  // Here, not when interned: the element Structure is complete only now.
  shim_header();
  shim += fmt("_CG_FFI_DEF int64_t _CG_ffi_sizeof_a%d(void) { return (int64_t)sizeof(%s); }\n", k, c_decl(*a, "").c_str());
  std::string t = fmt("class __pyc_ffi_A_%d__(object):\n", k);
  t += "  def __init__(self):\n";
  t += fmt("    self._buf = __pyc_c_call__(bytes, \"_CG_ffi_buf\", int, %s)\n", size_expr(*a).c_str());
  t += "    self._addr = __pyc_c_call__(int, \"_CG_ffi_bytes_addr\", bytes, self._buf)\n";
  // A(x, y) is A().__pyc_ffi_ini__(0, x).__pyc_ffi_ini__(1, y) (rewrite_ctors).
  t += "  def __pyc_ffi_ini__(self, i, v):\n";
  t += "    self.__setitem__(i, v)\n";
  t += "    return self\n";
  t += "  def __len__(self):\n";
  t += fmt("    return %d\n", n);
  t += "  def __getitem__(self, i):\n" + check + py_read_at(e, at, "self._buf", "    ");
  t += "  def __setitem__(self, i, v):\n" + check + py_write_at(e, at, "v", "    ");
  t += "  def __iter__(self):\n";
  t += fmt("    return iter([self.__getitem__(__pyc_j__) for __pyc_j__ in range(%d)])\n", n);
  t += fmt("  def __pyc_ffi_as_%d__(self):\n", pid);
  t += "    return self._addr\n";
  t += fmt("  def __pyc_ffi_aval_%d__(self):\n", k);
  t += "    return self._addr\n";
  // No byref: CPython's byref(a) is a pointer to the ARRAY, which a
  // POINTER(elem) parameter rejects; `a` itself converts.
  t += "  def __pyc_ffi_sizeof__(self):\n";
  t += "    return " + size_expr(*a) + "\n";
  if (e.kind == K_CHAR) {
    // CPython's c_char array: .value is the bytes to the first NUL, .raw all of them.
    t += "  def __pyc_get_value__(self):\n";
    t += fmt("    return __pyc_c_call__(bytes, \"_CG_ffi_rd_cstr_n\", int, self._addr, int, %d)\n", n);
    t += "  def __pyc_set_value__(self, v):\n" + py_write_at(*a, "self._addr", "v", "    ");
    t += "  def __pyc_get_raw__(self):\n";
    t += fmt("    return __pyc_c_call__(bytes, \"_CG_ffi_rd_raw\", int, self._addr, int, %d)\n", n);
    t += "  def __pyc_set_raw__(self, v):\n";
    t += fmt("    if len(v) > %d: raise ValueError(\"byte string too long\")\n", n);
    t += "    __pyc_c_call__(int, \"_CG_ffi_wr_chars\", int, self._addr, bytes, v, int, len(v), int, 0)\n";
    pyc_add_property_name("value");
    pyc_add_setter_name("value");
    pyc_add_property_name("raw");
    pyc_add_setter_name("raw");
  }
  t += fmt("def __pyc_ffi_aview_%d__(addr, keep):\n", k);
  t += fmt("  o = __pyc_ffi_A_%d__()\n", k);
  t += "  o._buf = keep\n";
  t += "  o._addr = addr\n";
  t += "  return o\n";
  return t;
}

void shim_fun(FLib *lib, FFun *f) {
  shim_header();
  // The foreign symbol, under a private name so no header can disagree.
  std::string decl = fmt("extern \"C\" %s _CG_ffi_d_%d_%s(", c_type(f->res).c_str(), lib->id, f->name.c_str());
  for (size_t i = 0; i < f->args.size(); i++) decl += std::string(i ? ", " : "") + c_type(f->args[i]);
  if (!f->args.size()) decl += "void";
  decl += fmt(") _CG_FFI_SYM(\"%s\");\n", f->name.c_str());
  shim += decl;
  bool sret = f->res.kind == K_STRUCT;
  std::string def = fmt("_CG_FFI_DEF %s _CG_ffi_%d_%s(", sret ? "int64_t" : word_type(f->res), lib->id, f->name.c_str());
  std::string call = fmt("_CG_ffi_d_%d_%s(", lib->id, f->name.c_str());
  bool first = true;
  if (sret) {
    def += "int64_t r";
    first = false;
  }
  for (size_t i = 0; i < f->args.size(); i++) {
    std::string a = fmt("a%d", (int)i);
    def += fmt("%s%s %s", first ? "" : ", ", word_type(f->args[i]), a.c_str());
    first = false;
    call += (i ? ", " : "") + c_from_word(f->args[i], a);
  }
  if (first) def += "void";
  call += ")";
  if (sret)
    def += fmt(") { *(%s *)(intptr_t)r = %s; return 0; }\n", c_type(f->res).c_str(), call.c_str());
  else if (f->res.kind == K_VOID)
    def += fmt(") { %s; return 0; }\n", call.c_str());
  else
    def += fmt(") { return %s; }\n", c_to_word(f->res, call).c_str());
  shim += def;
}

std::string py_lib(FLib *lib) {
  std::string t = fmt("class __pyc_ffi_lib_%d__:\n", lib->id);
  if (!lib->funs.size()) t += "  pass\n";
  for (FFun *f : lib->funs) {
    t += fmt("  def %s(self", f->name.c_str());
    for (size_t i = 0; i < f->args.size(); i++) t += fmt(", a%d", (int)i);
    t += "):\n";
    bool sret = f->res.kind == K_STRUCT;
    std::string call = fmt("__pyc_c_call__(%s, \"_CG_ffi_%d_%s\"", f->res.kind == K_FLOAT ? "float" : "int", lib->id,
                           f->name.c_str());
    if (sret) {
      t += fmt("    __pyc_r__ = %s()\n", f->res.s->name.c_str());
      call += ", int, __pyc_r__._addr";
    }
    for (size_t i = 0; i < f->args.size(); i++) {
      cchar *decl;
      std::string conv = py_arg(f->args[i], fmt("a%d", (int)i), decl);
      call += fmt(", %s, %s", decl, conv.c_str());
    }
    call += ")";
    if (sret)
      t += "    " + call + "\n    return __pyc_r__\n";
    else
      t += py_ret(f->res, call, "    ");
  }
  return t;
}

// Any use of a ctypes name that lowering did not consume is something this
// subset does not cover; say so here rather than let it fail obscurely.
void refuse_leftovers(Env &env, PyDAST *n) {
  if (!n) return;
  if (n->kind == PY_import_name || n->kind == PY_import_from) return;
  if (n->kind == PY_power && n->children.n >= 2) {
    // lib.NAME(...) for an undeclared NAME
    PyDAST *h = n->children[0], *a = n->children[1];
    if (h->kind == PY_name && h->str_val && a->kind == PY_attribute && a->children.n && a->children[0]->str_val) {
      auto it = env.libs.find(h->str_val);
      if (it != env.libs.end()) {
        bool found = false;
        for (FFun *f : it->second->funs) found = found || f->name == a->children[0]->str_val;
        if (!found)
          err(env, n, "%s.%s is used but has no argtypes; pyc needs every foreign function's argtypes declared",
              h->str_val, a->children[0]->str_val);
      }
    }
    int nref = 1;
    while (nref < n->children.n && n->children[nref]->kind == PY_attribute) nref++;
    std::string r = ctypes_ref(env, n, nref);
    if (r != "" && r != "." && !is_runtime_name(r))
      err(env, n, "this use of ctypes.%s is outside the static subset pyc compiles (issues/188)", r.c_str());
    for (int i = r != "" ? nref : 0; i < n->children.n; i++) refuse_leftovers(env, n->children[i]);
    return;
  }
  if (n->kind == PY_name && n->str_val && n->ctx == PY_LOAD) {
    std::string r = ctypes_ref(env, n);
    if (r != "" && r != "." && !is_runtime_name(r))
      err(env, n, "this use of ctypes.%s is outside the static subset pyc compiles (issues/188)", r.c_str());
    return;
  }
  if (n->kind == PY_attribute || n->kind == PY_keyword_arg) {
    // the attribute / keyword NAME is not a reference; a keyword's value is
    for (int i = 1; i < n->children.n; i++) refuse_leftovers(env, n->children[i]);
    return;
  }
  if (n->kind == PY_funcdef || n->kind == PY_classdef) {
    for (int i = 1; i < n->children.n; i++) refuse_leftovers(env, n->children[i]);
    return;
  }
  for (PyDAST *c : n->children) refuse_leftovers(env, c);
}

void lower_module(PyDAST *mod, cchar *filename) {
  if (!mod || mod->kind != PY_module) return;
  Env env;
  env.filename = filename;
  for (PyDAST *st : mod->children) {
    if (st->kind == PY_import_name || st->kind == PY_import_from)
      scan_import(env, st);
    else if (st->kind == PY_classdef)
      scan_class(env, st);
    else if (st->kind == PY_assign)
      scan_assign(env, st);
  }
  if (!env.mod_names.size() && !env.from.size() && !env.star) return;
  rewrite_ctors(env, mod);
  for (FLib *lib : env.lib_order)
    for (FFun *f : lib->funs)
      if (!f->has_args)
        fail("error %s:%d: ctypes: %s.%s has no argtypes; pyc needs every foreign function's argtypes declared "
             "(an empty list for a function of no arguments)",
             f->filename, f->line, lib->var.c_str(), f->name.c_str());
  // Splice the generated code in around the statements it replaces.
  std::map<PyDAST *, std::vector<PyDAST *>> before, after;
  std::map<PyDAST *, PyDAST *> replace;
  for (FStruct *s : env.struct_order) {
    shim_struct(s);
    // Its pointer types follow it, innermost first (ids are in that order).
    std::string ptrs;
    for (FType *p : env.ptr_order)
      if (ptr_root(p) == s) ptrs += py_pointer(p);
    for (FType *a : env.arr_order)
      if (ptr_root(a) == s) ptrs += py_array(a);
    PyDAST *g = parse_generated(env, py_struct(s) + ptrs, s->cdef);
    bool first = true;
    for (PyDAST *c : g->children) {
      if (first) {
        // The user's own statements (methods, a docstring) join the body.
        Vec<PyDAST *> user;
        body_statements(s->cdef, user);
        PyDAST *body = c->children.last();
        for (PyDAST *u : user)
          if (u->kind != PY_pass_stmt) body->add(u);
        replace[s->cdef] = c;
        first = false;
      } else
        after[s->cdef].push_back(c);
    }
  }
  for (FLib *lib : env.lib_order) {
    for (FFun *f : lib->funs) shim_fun(lib, f);
    PyDAST *g = parse_generated(env, py_lib(lib) + fmt("__pyc_x__ = __pyc_ffi_lib_%d__()\n", lib->id), lib->stmt);
    if (g->children.n != 2 || g->children[1]->kind != PY_assign) fail("internal error: ctypes library class shape");
    // A diagnostic inside a wrapper names the function's own declaration.
    {
      size_t fi = 0;
      for (PyDAST *m : g->children[0]->children.last()->children) {
        if (m->kind != PY_funcdef || !m->children.n || !m->children[0]->str_val) continue;
        if (fi < lib->funs.size()) set_origin(m, lib->funs[fi]->filename, lib->funs[fi]->line);
        fi++;
      }
    }
    before[lib->stmt].push_back(g->children[0]);
    PyDAST *value = g->children[1]->children[1];
    value->parent = lib->stmt;
    lib->stmt->children.v[1] = value;
  }
  Vec<PyDAST *> out;
  for (PyDAST *st : mod->children) {
    for (PyDAST *b : before[st]) out.add(b);
    out.add(replace.count(st) ? replace[st] : st);
    for (PyDAST *a : after[st]) out.add(a);
  }
  mod->children.clear();
  for (PyDAST *st : out) mod->add(st);
  refuse_leftovers(env, mod);
}

// The support classes, and the conversion methods on the builtin types an
// argument may have. Added to the builtin module only for a program that
// imports ctypes, so no other program sees them.
void inject_builtin(PyDAST *builtin) {
  static const char *fixed =
      "class __pyc_ffi_pointer__(object):\n"
      "  def __init__(self):\n"
      "    self._buf = __pyc_c_call__(bytes, \"_CG_ffi_buf\", int, __pyc_c_call__(int, \"_CG_ffi_sizeof_ptr\"))\n"
      "    self._addr = __pyc_c_call__(int, \"_CG_ffi_bytes_addr\", bytes, self._buf)\n"
      "  def __bool__(self):\n"
      "    return __pyc_c_call__(int, \"_CG_ffi_rd_ptr\", int, self._addr) != 0\n"
      "  def __pyc_ffi_addr__(self):\n"
      "    return __pyc_c_call__(int, \"_CG_ffi_rd_ptr\", int, self._addr)\n"
      "  def __pyc_ffi_sizeof__(self):\n"
      "    return __pyc_c_call__(int, \"_CG_ffi_sizeof_ptr\")\n"
      "class __pyc_ffi_ref__(object):\n"
      "  def __init__(self, a, keep):\n"
      "    self._addr = a\n"
      "    self._keep = keep\n"
      "  def __pyc_ffi_addr__(self):\n"
      "    return self._addr\n"
      "class __pyc_ffi_struct__(object):\n"
      "  pass\n"
      "class __pyc_ffi_inject_int__:\n"
      "  def __pyc_ffi_addr__(self):\n"
      "    return self\n"
      "class __pyc_ffi_inject_bool__:\n"
      "  pass\n"
      "class __pyc_ffi_inject_float__:\n"
      "  pass\n"
      "class __pyc_ffi_inject_bytes__:\n"
      "  def __pyc_ffi_charp__(self):\n"
      "    return __pyc_c_call__(int, \"_CG_ffi_bytes_addr\", bytes, self)\n"
      "  def __pyc_ffi_addr__(self):\n"
      "    return __pyc_c_call__(int, \"_CG_ffi_bytes_addr\", bytes, self)\n";
  std::string text = fixed;
  // The argument conversions, one method per canonical scalar class, so that
  // a box converts only where its own class is declared.
  std::string on_int, on_bool, on_float, on_bytes;
  for (const Scalar &sc : scalars) {
    if (strcmp(sc.name, sc.canon)) continue;
    std::string m = fmt("  def __pyc_ffi_%s__(self):\n", sc.name);
    switch (sc.kind) {
      case K_INT: case K_BOOL: case K_CHAR:
        on_int += m + "    return self\n";
        on_bool += m + "    return int(self)\n";
        if (sc.kind == K_CHAR)
          on_bytes += m + "    if len(self) != 1: raise TypeError(\"one character bytes, bytearray or integer expected\")\n" +
                      "    return self[0]\n";
        break;
      case K_FLOAT:
        on_int += m + "    return float(self)\n";
        on_bool += m + "    return float(int(self))\n";
        on_float += m + "    return self\n";
        break;
      default: break;
    }
  }
  text += "class __pyc_ffi_inject_int__:\n" + on_int + "class __pyc_ffi_inject_bool__:\n" + on_bool +
          "class __pyc_ffi_inject_float__:\n" + on_float + "class __pyc_ffi_inject_bytes__:\n" + on_bytes;
  // Pointers to scalars, and the boxes: shared by every module.
  for (FType *p : ptr_types)
    if (!ptr_root(p)) text += py_pointer(p);
  for (FType *a : arr_types)
    if (!ptr_root(a)) text += py_array(a);
  for (FBox *b : boxes) text += py_box(b);
  // The pointer classes raise (NULL access) and the boxes call conversions
  // that do, from code the program reaches by `p[i]` and `b.value`, which
  // the syntactic gate (user_code_reaches_raise) cannot see: builtin code is
  // not scanned for it. The program that uses them can raise.
  if (boxes.size() || arr_types.size()) pyc_program_has_raise = true;
  if (getenv("PYC_DBG_CTYPES")) fprintf(stderr, "--- ctypes: builtin\n%s", text.c_str());
  PyDAST *g = dparse_python_buf_to_ast("<ctypes>", text.c_str(), (int)text.size());
  if (!g) fail("internal error: ctypes support code does not parse");
  for (PyDAST *c : g->children) {
    if (c->kind == PY_funcdef) {  // a view constructor, __pyc_ffi_mk_K__
      builtin->add(c);
      continue;
    }
    if (c->kind != PY_classdef || !c->children.n || !c->children[0]->str_val) continue;
    cchar *name = c->children[0]->str_val;
    static const char *prefix = "__pyc_ffi_inject_";
    if (strncmp(name, prefix, strlen(prefix))) {
      builtin->add(c);
      continue;
    }
    std::string target(name + strlen(prefix), strlen(name) - strlen(prefix) - 2);
    PyDAST *into = nullptr;
    for (PyDAST *b : builtin->children)
      if (b->kind == PY_classdef && b->children.n && b->children[0]->str_val && target == b->children[0]->str_val)
        into = b;
    if (!into) fail("internal error: ctypes: no builtin class '%s'", target.c_str());
    for (PyDAST *m : c->children.last()->children)
      if (m->kind == PY_funcdef) into->children.last()->add(m);
  }
}

}  // namespace

cchar *pyc_ffi_shim = nullptr;

void lower_ctypes(Vec<PycModule *> &mods, Vec<PyDAST *> &imported) {
  for (int i = 1; i < mods.n; i++) lower_module(mods[i]->pymod, mods[i]->filename);
  for (PyDAST *a : imported) lower_module(a, a && a->filename ? a->filename : "<imported module>");
  if (!builtin_needed) return;
  inject_builtin(mods[0]->pymod);
  if (shim.size()) {
    pyc_ffi_shim = dupstr(shim.c_str());
    codegen_extra_source = dupstr(("#define _CG_FFI_DEF extern \"C\"\n" + shim).c_str());
  }
}
