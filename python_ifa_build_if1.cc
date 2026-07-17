// SPDX-License-Identifier: BSD-3-Clause
#include "python_ifa_int.h"
#include "python_parse.h"
#include <functional>

static int build_if1_pyda(PyDAST *n, PycCompiler &ctx);
static void emit_assign_to_target(PyDAST *tgt, Sym *val, Code **code, PycAST *ast, PycCompiler &ctx);

static char *pyda_trim(const char *s) {
  if (!s) return nullptr;
  int len = strlen(s);
  while (len > 0 && isspace((unsigned char)s[len-1])) len--;
  char *r = (char *)GC_malloc(len + 1);
  memcpy(r, s, len);
  r[len] = 0;
  return r;
}

static void build_import_if1(char *sym, char *as, char *from, PycCompiler &ctx) {
  // str_val from DParser non-terminal (dotted_name) may have trailing whitespace
  if (sym) sym = pyda_trim(sym);
  if (from) from = pyda_trim(from);
  char *mod = from ? from : sym;
  if (!strcmp(mod, "pyc_compat")) return;
  PycModule *m = get_module(mod, ctx);
  // Mirror build_import_syms' dotted-import fallback: `import os.path`
  // with no `os.path.py` on the search path resolves to the `os`
  // module (see build_import_syms for the rationale).
  if (!m && !from) {
    char *dot = strchr(mod, '.');
    if (dot) {
      char *top = (char *)GC_malloc(dot - mod + 1);
      memcpy(top, mod, dot - mod);
      top[dot - mod] = 0;
      m = get_module(top, ctx);
    }
  }
  // build_import_syms only registers a module if it found <mod>.py on
  // the search path; a missing module (typically an unshimmed stdlib
  // module -- time/random/math/sys/copy/...) left `m` null and this
  // used to abort the whole compiler with assert(m). Importing a
  // module pyc doesn't provide is a user-facing condition, not an
  // internal invariant: emit a clean diagnostic instead of SIGABRT.
  if (!m) fail("error line %d, cannot find module '%s' (no '%s.py' on the search path; "
               "pyc does not yet provide this module)", ctx.lineno, mod, mod);
  if (!m->built_if1) {
    // ctx.node is the import statement node whose ast->code the enclosing
    // statement loop consumes; the module builders below re-point ctx.node
    // at the imported module's scope node (enter_scope(PyDAST*)) without
    // restoring, so a second import in the same statement (`import a, b`)
    // would splice b's code into a's dead module PycAST. Restore it.
    void *saved_node = ctx.node;
    Code **c;
    c = &getAST((PyDAST *)ctx.node, ctx)->code;
    build_module_attributes_if1(m, *m->ctx, c);
    build_if1_module_pyda(m->pymod, *m->ctx, c);
    m->built_if1 = true;
    ctx.node = saved_node;
  }
}

// ifa/issues/031 step 2: is this Sym a module-level *data*
// variable (a mutable global cell)? Reads of these are routed
// through a per-read local temp (see PY_name in build_if1_pyda)
// so each read site gets an EntrySet-contoured, SSU-renamable
// Var instead of the single shared GLOBAL_CONTOUR AVar.
// Excludes everything whose identity downstream passes match on:
// functions (call dispatch), classes/types (instantiation,
// isinstance), modules (attribute resolution), interned symbols,
// constants (True/False/literals), and builtin unique objects
// (None/nil, void, unknown -- `is_external`/`is_builtin`; codegen's
// constant-elision protocol assumes consumers reference these
// directly, so a load temp for them would be declared but never
// defined).
static bool is_module_data_var(Sym *s) {
  return s && s->nesting_depth == 0 && s->name && !s->is_fun && !s->is_module && !s->is_symbol && !s->is_constant &&
         !s->constant && !s->type_kind && !s->is_meta_type && !s->is_external && !s->is_builtin;
}

static Code *find_send(Code *c) {
  if (c->kind == Code_SEND) return c;
  assert(c->kind == Code_SUB);
  for (int i = c->sub.n - 1; i >= 0; i--) {
    if (c->sub[i]->kind == Code_SUB) {
      Code *cc = find_send(c->sub[i]);
      if (cc) return cc;
    }
    if (c->sub[i]->kind == Code_SEND) return c->sub[i];
  }
  return 0;
}


// ---- PyDAST (pyda) build_if1 path ----

static Sym *map_pyop_to_operator(int op) {
  switch (op) {
    case PY_OP_ADD: return make_symbol("__add__");
    case PY_OP_SUB: return make_symbol("__sub__");
    case PY_OP_MUL: return make_symbol("__mul__");
    case PY_OP_DIV: return make_symbol("__truediv__");
    case PY_OP_MOD: return make_symbol("__mod__");
    case PY_OP_POW: return make_symbol("__pow__");
    case PY_OP_LSHIFT: return make_symbol("__lshift__");
    case PY_OP_RSHIFT: return make_symbol("__rshift__");
    case PY_OP_BITOR: return make_symbol("__or__");
    case PY_OP_BITXOR: return make_symbol("__xor__");
    case PY_OP_BITAND: return make_symbol("__and__");
    case PY_OP_FLOORDIV: return make_symbol("__floordiv__");
    default: assert(!"unhandled pyop"); return nullptr;
  }
}

static Sym *map_pyop_to_ioperator(int op) {
  switch (op) {
    case PY_OP_ADD: return make_symbol("__iadd__");
    case PY_OP_SUB: return make_symbol("__isub__");
    case PY_OP_MUL: return make_symbol("__imul__");
    case PY_OP_DIV: return make_symbol("__itruediv__");
    case PY_OP_MOD: return make_symbol("__imod__");
    case PY_OP_POW: return make_symbol("__ipow__");
    case PY_OP_LSHIFT: return make_symbol("__ilshift__");
    case PY_OP_RSHIFT: return make_symbol("__irshift__");
    case PY_OP_BITOR: return make_symbol("__ior__");
    case PY_OP_BITXOR: return make_symbol("__ixor__");
    case PY_OP_BITAND: return make_symbol("__iand__");
    case PY_OP_FLOORDIV: return make_symbol("__ifloordiv__");
    default: assert(!"unhandled pyop"); return nullptr;
  }
}

static Sym *map_pyop_to_unary(int op) {
  switch (op) {
    case PY_OP_UADD: return make_symbol("__pos__");
    case PY_OP_USUB: return make_symbol("__neg__");
    case PY_OP_INVERT: return make_symbol("__invert__");
    default: assert(!"unhandled pyop"); return nullptr;
  }
}

static Sym *map_pyop_to_cmp(int op) {
  switch (op) {
    case PY_CMP_EQ: return make_symbol("__eq__");
    case PY_CMP_NE: return make_symbol("__ne__");
    case PY_CMP_LT: return make_symbol("__lt__");
    case PY_CMP_LE: return make_symbol("__le__");
    case PY_CMP_GT: return make_symbol("__gt__");
    case PY_CMP_GE: return make_symbol("__ge__");
    case PY_CMP_IS: return make_symbol("__is__");
    case PY_CMP_IS_NOT: return make_symbol("__nis__");
    case PY_CMP_IN: return make_symbol("__contains__");
    case PY_CMP_NOT_IN: return make_symbol("__ncontains__");
    default: assert(!"unhandled cmpop"); return nullptr;
  }
}

// `x in y` / `x not in y` are asymmetric: unlike every other comparison
// operator (where the left operand is always the dispatch receiver), `in`
// calls the *container's* (right operand's) __contains__ with the item
// (left operand) as the argument. There is no real `__ncontains__` dunder
// in Python (map_pyop_to_cmp's PY_CMP_NOT_IN mapping to that name is never
// actually reached — this helper intercepts both PY_CMP_IN/PY_CMP_NOT_IN
// before either comparison call site falls through to map_pyop_to_cmp);
// `not in` is `__contains__` followed by `__not__`, mirroring how `is not`
// is already lowered a few lines up.
static void emit_in_pyda(Code **code, PycAST *ast, Sym *item, Sym *container, int op, Sym *result) {
  if (op == PY_CMP_IN) {
    call_method(code, ast, container, make_symbol("__contains__"), result, 1, item);
  } else {
    Sym *tmp = new_sym(ast);
    call_method(code, ast, container, make_symbol("__contains__"), tmp, 1, item);
    if1_send(if1, code, 2, 1, make_symbol("__not__"), tmp, result)->ast = ast;
  }
}

// Parse a PY_number PyDAST node as a decimal/hex/octal integer
// literal. Mirrors the int-literal branch of make_num_pyda further
// below: the grammar doesn't reliably populate is_int/int_val (see
// that function's comment), so this re-parses str_val directly.
// Returns false for float/imaginary literals or a non-PY_number
// node.
static bool try_int_literal(PyDAST *n, long *out) {
  if (!n || n->kind != PY_number) return false;
  const char *s = n->str_val;
  if (!s) return false;
  for (const char *p = s; *p; p++)
    if (*p == '.' || *p == 'e' || *p == 'E' || *p == 'j' || *p == 'J') return false;
  char *end;
  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) *out = strtol(s + 2, &end, 16);
  else if (s[0] == '0' && s[1] >= '0' && s[1] <= '7') *out = strtol(s + 1, &end, 8);
  else *out = strtol(s, &end, 10);
  return true;
}

// issue 025 R1 "tuple concatenation/repetition": tuples are
// fixed-arity structs, so `tup1 + tup2` / `tup * n` have no general
// runtime implementation (each length is a distinct struct type,
// unlike list/str). When every length involved is known at COMPILE
// time -- literal tuples combined via `+` and `*` against an
// integer literal, any nesting depth (chess.py's board-setup
// literal is exactly this shape) -- the whole expression can be
// flattened into a single tuple literal instead, sidestepping the
// missing runtime op entirely.
//
// Collects the flattened element list into `out` and returns true
// when `n` matches this shape; returns false (leaving normal
// __add__/__mul__ dispatch to run and fail as before, e.g. for a
// non-constant repeat count or a tuple + non-tuple) otherwise.
// Element PyDAST* pointers may repeat in `out` -- Python's `expr *
// n` evaluates expr's tuple ONCE and repeats REFERENCES to its
// already-computed elements, it does not re-execute the element
// expressions -- callers must build_if1_pyda each DISTINCT pointer
// exactly once (first-occurrence order) and then reference its rval
// for every position it appears at.
static bool try_fold_tuple_arity(PyDAST *n, Vec<PyDAST *> &out) {
  if (!n) return false;
  if (n->kind == PY_tuple) {
    for (auto c : n->children.values()) out.add(c);
    return true;
  }
  if (n->kind != PY_binop) return false;
  if (n->op == PY_OP_ADD) {
    Vec<PyDAST *> l, r;
    if (!try_fold_tuple_arity(n->children[0], l)) return false;
    if (!try_fold_tuple_arity(n->children[1], r)) return false;
    for (auto c : l.values()) out.add(c);
    for (auto c : r.values()) out.add(c);
    return true;
  }
  if (n->op == PY_OP_MUL) {
    Vec<PyDAST *> base;
    PyDAST *count_side = nullptr;
    if (try_fold_tuple_arity(n->children[0], base)) {
      count_side = n->children[1];
    } else {
      Vec<PyDAST *> base2;
      if (!try_fold_tuple_arity(n->children[1], base2)) return false;
      base = base2;
      count_side = n->children[0];
    }
    long reps;
    if (!try_int_literal(count_side, &reps)) return false;
    if (reps < 0) reps = 0;
    for (long i = 0; i < reps; i++)
      for (auto c : base.values()) out.add(c);
    return true;
  }
  return false;
}

// Make a number constant from a PyDAST PY_number node
static Sym *make_num_pyda(PyDAST *n, PycCompiler &ctx) {
  // Parse str_val since is_int/int_val/float_val may not be set from grammar
  const char *s = n->str_val;
  if (!s) s = "0";
  // Determine type: float if contains '.', 'e', 'E', or 'j'/'J' (imaginary)
  bool is_float = false;
  for (const char *p = s; *p; p++) {
    if (*p == '.' || *p == 'e' || *p == 'E') { is_float = true; break; }
    if (*p == 'j' || *p == 'J') { is_float = true; break; }
  }
  if (n->is_int) {
    Immediate imm;
    imm.v_int64 = (int64)n->int_val;
    char buf[80];
    sprintf(buf, "%ld", n->int_val);
    return if1_const(if1, sym_int64, buf, &imm);
  } else if (!is_float) {
    // Integer: parse hex/oct/decimal
    char *end;
    int64 v;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
      v = (int64)strtoul(s + 2, &end, 16);
    else if (s[0] == '0' && s[1] >= '0' && s[1] <= '7')
      v = (int64)strtoul(s + 1, &end, 8);
    else
      v = strtol(s, &end, 10);
    // Skip trailing 'l'/'L'
    Immediate imm;
    imm.v_int64 = v;
    char buf[80];
    sprintf(buf, "%ld", (long)v);
    return if1_const(if1, sym_int64, buf, &imm);
  } else {
    Immediate imm;
    imm.v_float64 = strtod(s, nullptr);
    char buf[80];
    sprintf(buf, "%g", imm.v_float64);
    return if1_const(if1, sym_float64, buf, &imm);
  }
}

// Parse a Python string literal into its actual string value (pure C, no CPython runtime needed)
// Skip a string-literal prefix (any combination of r/R/b/B/u/U/f/F) and
// classify it. Returns a pointer to the opening quote character.
static const char *skip_string_prefix(const char *s, bool *is_raw, bool *is_fstring) {
  *is_raw = false;
  *is_fstring = false;
  while (*s && (*s == 'r' || *s == 'R' || *s == 'b' || *s == 'B' ||
                *s == 'u' || *s == 'U' || *s == 'f' || *s == 'F')) {
    if (*s == 'r' || *s == 'R') *is_raw = true;
    if (*s == 'f' || *s == 'F') *is_fstring = true;
    s++;
  }
  return s;
}

// Decode escape sequences in [s, end) (or copy verbatim if is_raw). Shared
// by plain string literals and by each literal run between `{expr}`
// interpolations in an f-string.
static char *decode_string_content(const char *s, const char *end, bool is_raw) {
  int content_len = (int)(end - s);
  char *out = (char *)MALLOC(content_len + 1);
  if (is_raw) {
    memcpy(out, s, content_len);
    out[content_len] = 0;
    return out;
  }
  char *op = out;
  for (const char *p = s; p < end; ) {
    if (*p != '\\') { *op++ = *p++; continue; }
    p++;  // skip backslash
    if (p >= end) break;
    switch (*p) {
      case '\\': *op++ = '\\'; p++; break;
      case '\'': *op++ = '\''; p++; break;
      case '"':  *op++ = '"';  p++; break;
      case 'n':  *op++ = '\n'; p++; break;
      case 't':  *op++ = '\t'; p++; break;
      case 'r':  *op++ = '\r'; p++; break;
      case 'b':  *op++ = '\b'; p++; break;
      case 'f':  *op++ = '\f'; p++; break;
      case 'v':  *op++ = '\v'; p++; break;
      case 'a':  *op++ = '\a'; p++; break;
      case '0':  *op++ = '\0'; p++; break;
      case 'x': {
        p++;
        int v = 0;
        for (int i = 0; i < 2 && p < end && isxdigit((unsigned char)*p); i++, p++) {
          v = v * 16 + (isdigit((unsigned char)*p) ? *p - '0' : tolower((unsigned char)*p) - 'a' + 10);
        }
        *op++ = (char)v; break;
      }
      case 'u': case 'U': {
        int ndigits = (*p == 'u') ? 4 : 8;
        p++;
        int v = 0;
        for (int i = 0; i < ndigits && p < end && isxdigit((unsigned char)*p); i++, p++)
          v = v * 16 + (isdigit((unsigned char)*p) ? *p - '0' : tolower((unsigned char)*p) - 'a' + 10);
        // Encode as UTF-8
        if (v < 0x80) { *op++ = (char)v; }
        else if (v < 0x800) { *op++ = (char)(0xC0 | (v >> 6)); *op++ = (char)(0x80 | (v & 0x3F)); }
        else if (v < 0x10000) { *op++ = (char)(0xE0 | (v >> 12)); *op++ = (char)(0x80 | ((v >> 6) & 0x3F)); *op++ = (char)(0x80 | (v & 0x3F)); }
        else { *op++ = (char)(0xF0 | (v >> 18)); *op++ = (char)(0x80 | ((v >> 12) & 0x3F)); *op++ = (char)(0x80 | ((v >> 6) & 0x3F)); *op++ = (char)(0x80 | (v & 0x3F)); }
        break;
      }
      case 'N': { // \N{name} — skip name, output replacement char
        if (p[1] == '{') { while (p < end && *p != '}') p++; if (p < end) p++; }
        *op++ = '?'; break;
      }
      default:
        // Octal or unknown: \ooo
        if (*p >= '0' && *p <= '7') {
          int v = 0;
          for (int i = 0; i < 3 && p < end && *p >= '0' && *p <= '7'; i++, p++)
            v = v * 8 + (*p - '0');
          *op++ = (char)v;
        } else {
          *op++ = '\\'; *op++ = *p++;  // keep literal backslash
        }
        break;
    }
  }
  *op = 0;
  return out;
}

static Sym *eval_string_pyda(PyDAST *n, PycCompiler &ctx) {
  const char *s = n->str_val;
  if (!s || !*s) return make_string("");
  bool is_raw, is_fstring;
  s = skip_string_prefix(s, &is_raw, &is_fstring);
  // Determine quote character and whether triple-quoted
  char q = *s;
  if (q != '\'' && q != '"') return make_string(s);  // shouldn't happen
  s++;
  bool triple = (s[0] == q && s[1] == q);
  if (triple) s += 2;
  // Find end of string content
  const char *end = s;
  if (triple) {
    while (*end && !(end[0] == q && end[1] == q && end[2] == q)) end++;
  } else {
    while (*end && *end != q && *end != '\n') end++;
  }
  return make_string(decode_string_content(s, end, is_raw));
}

// Append `piece` (a string-typed Sym) to the running f-string concatenation
// `*result`, emitting a `__add__` send into `ast->code` when there's a prior
// piece to concatenate with.
static void fstring_append_piece(PycAST *ast, PycCompiler &ctx, Sym **result, Sym *piece) {
  if (!*result) { *result = piece; return; }
  Sym *next = new_sym(ast);
  call_method(&ast->code, ast, *result, make_symbol("__add__"), next, 1, piece);
  *result = next;
}

// Parse `src` (raw Python expression source, e.g. the text of an f-string's
// `{expr}` field) as a standalone expression, build its symbols/IF1 code at
// the CURRENT scope (ctx.scope_stack), and return its PycAST. Wraps `src` in
// parens so it parses as one `file_input` -> `expr_stmt` -> expression.
static PycAST *build_fstring_subexpr_pyda(const char *src, int lineno, PycCompiler &ctx) {
  int len = (int)strlen(src);
  char *buf = (char *)MALLOC(len + 3);
  buf[0] = '(';
  memcpy(buf + 1, src, len);
  buf[len + 1] = ')';
  buf[len + 2] = '\n';
  PyDAST *mod = dparse_python_buf_to_ast("<fstring>", buf, len + 3);
  if (!mod || mod->kind != PY_module || mod->children.n != 1 ||
      mod->children[0]->kind != PY_expr_stmt || mod->children[0]->children.n != 1)
    fail("error line %d, invalid expression in f-string field: '%s'", lineno, src);
  PyDAST *expr = mod->children[0]->children[0];
  build_syms_pyda(expr, ctx);
  build_if1_pyda(expr, ctx);
  return getAST(expr, ctx);
}

// Scan one `{...}` replacement field starting just after the opening `{`
// (i.e. *p is the first character of the field). On return, `*p` points
// just past the field's closing `}`. Splits off an optional `!s`/`!r`/`!a`
// conversion and an optional `:spec` format spec, both recognized only at
// nesting depth 0 relative to the field (bracket/paren/brace nesting and
// quoted sub-strings are skipped over so slices, dict/set literals, and
// lambdas inside the field don't confuse the scan).
static void scan_fstring_field(const char **pp, const char *end, int lineno,
                                char **out_expr, char *out_conv, char **out_spec) {
  const char *start = *pp;
  const char *p = *pp;
  const char *expr_end = nullptr;
  *out_conv = 0;
  *out_spec = nullptr;
  int depth = 0;
  while (p < end) {
    char c = *p;
    if (c == '\'' || c == '"') {
      char qc = c;
      p++;
      while (p < end && *p != qc) { if (*p == '\\' && p + 1 < end) p++; p++; }
      if (p < end) p++;
      continue;
    }
    if (c == '(' || c == '[' || c == '{') { depth++; p++; continue; }
    if (c == ')' || c == ']') { depth--; p++; continue; }
    if (c == '}') {
      if (depth == 0) { if (!expr_end) expr_end = p; p++; break; }
      depth--; p++; continue;
    }
    if (depth == 0 && c == '!' && p + 1 < end &&
        (p[1] == 's' || p[1] == 'r' || p[1] == 'a') &&
        p + 2 < end && (p[2] == ':' || p[2] == '}')) {
      if (!expr_end) expr_end = p;
      *out_conv = p[1];
      p += 2;
      continue;
    }
    if (depth == 0 && c == ':' && !*out_spec) {
      if (!expr_end) expr_end = p;
      const char *spec_start = p + 1;
      // Consume the rest of the field as the format spec (nested `{`/`}` in
      // a dynamic format spec, e.g. `{x:{width}}`, aren't unpacked here).
      int sdepth = 0;
      p++;
      while (p < end) {
        if (*p == '{') sdepth++;
        else if (*p == '}') { if (sdepth == 0) break; sdepth--; }
        p++;
      }
      int slen = (int)(p - spec_start);
      char *spec = (char *)MALLOC(slen + 1);
      memcpy(spec, spec_start, slen);
      spec[slen] = 0;
      *out_spec = spec;
      continue;
    }
    p++;
  }
  if (!expr_end) fail("error line %d, unterminated '{' in f-string", lineno);
  int elen = (int)(expr_end - start);
  char *expr = (char *)MALLOC(elen + 1);
  memcpy(expr, start, elen);
  expr[elen] = 0;
  *out_expr = expr;
  *pp = p;
}

// f-strings (PEP 498). Splits the (already prefix/quote-stripped) content
// into literal runs and `{expr}` fields, decodes each literal run through
// decode_string_content (with `{{`/`}}` collapsed to a literal brace first),
// lowers each field's expression via build_fstring_subexpr_pyda, stringifies
// it (str() for no/`!s` conversion, repr() for `!r`; `!a` also uses repr()
// since no separate ascii-escaping is implemented), and concatenates every
// piece left-to-right with `__add__`. A non-empty format spec (`{x:spec}`)
// dispatches to `__format__` (mirroring CPython's `format(x, spec)`):
// if a conversion (`!r`/`!s`/`!a`) was also given, the spec formats the
// already-converted *string*, not the original value, matching CPython's
// order of operations.
static void build_fstring_pyda(PyDAST *n, PycAST *ast, PycCompiler &ctx) {
  const char *s = n->str_val;
  bool is_raw, is_fstring;
  s = skip_string_prefix(s, &is_raw, &is_fstring);
  char q = *s;
  if (q != '\'' && q != '"') { ast->rval = make_string(s); return; }
  s++;
  bool triple = (s[0] == q && s[1] == q);
  if (triple) s += 2;
  const char *end = s;
  if (triple) {
    while (*end && !(end[0] == q && end[1] == q && end[2] == q)) end++;
  } else {
    while (*end && *end != q && *end != '\n') end++;
  }

  Sym *result = nullptr;
  Vec<char> lit;
  const char *p = s;
  auto flush_literal = [&]() {
    if (!lit.n) return;
    char *decoded = decode_string_content(lit.v, lit.v + lit.n, is_raw);
    fstring_append_piece(ast, ctx, &result, make_string(decoded));
    lit.clear();
  };
  while (p < end) {
    if (*p == '{' && p + 1 < end && p[1] == '{') { lit.add('{'); p += 2; continue; }
    if (*p == '}' && p + 1 < end && p[1] == '}') { lit.add('}'); p += 2; continue; }
    if (*p == '{') {
      flush_literal();
      p++;
      char *expr_src; char conv; char *spec;
      scan_fstring_field(&p, end, ctx.lineno, &expr_src, &conv, &spec);
      PycAST *e_ast = build_fstring_subexpr_pyda(expr_src, ctx.lineno, ctx);
      if1_gen(if1, &ast->code, e_ast->code);
      Sym *str_piece = new_sym(ast);
      cchar *method_name = (conv == 'r' || conv == 'a') ? "__repr__" : "__str__";
      if (spec && *spec) {
        Sym *spec_sym = make_string(spec);
        if (conv) {
          Sym *converted = new_sym(ast);
          call_method(&ast->code, ast, e_ast->rval, make_symbol(method_name), converted, 0);
          call_method(&ast->code, ast, converted, make_symbol("__format__"), str_piece, 1, spec_sym);
        } else {
          call_method(&ast->code, ast, e_ast->rval, make_symbol("__format__"), str_piece, 1, spec_sym);
        }
      } else {
        call_method(&ast->code, ast, e_ast->rval, make_symbol(method_name), str_piece, 0);
      }
      fstring_append_piece(ast, ctx, &result, str_piece);
      continue;
    }
    lit.add(*p);
    p++;
  }
  flush_literal();
  ast->rval = result ? result : make_string("");
}

// Check for and handle builtin function calls (super, __pyc_symbol__, etc.)
static int build_builtin_call_pyda(PycAST *atom_ast, PyDAST *call_trailer, PycAST *ast, PycCompiler &ctx) {
  Sym *f = atom_ast->sym;
  // Collect positional args from the call trailer
  Vec<PyDAST *> pos_args;
  if (call_trailer && call_trailer->children.n > 0) {
    PyDAST *arglist = call_trailer->children[0];
    for (auto arg : arglist->children.values())
      if (arg->kind != PY_keyword_arg) pos_args.add(arg);
  }
  // issues/020: str(x) -- unlike print/super/etc., `str` is a real class
  // (__pyc__/01_str.py), not a compiler-level builtin Sym, so it isn't in
  // builtin_functions and must be resolved by name here (same pattern as
  // PY_dict/PY_set resolving "dict"/"set" in build_syms_pyda). `class str`
  // has no __init__ that accepts a value to convert, so a 1-arg call would
  // otherwise fall through to the generic constructor-call path and silently
  // fail to consume its argument.
  if (f && pos_args.n == 1 && f->name && !strcmp(f->name, "str")) {
    PycSymbol *str_cls = make_PycSymbol(ctx, "str", PYC_USE);
    if (str_cls && f == str_cls->sym) {
      PycAST *a0 = getAST(pos_args[0], ctx);
      ast->rval = new_sym(ast);
      call_method(&ast->code, ast, a0->rval, sym___str__, ast->rval, 0);
      return 1;
    }
  }
  // issue 025 "has no type" bucket: list(iterable) -- `list` is a
  // builtin primitive type with no __init__ that accepts an
  // iterable, so `list(range(n))` (collatz's first statement) fell
  // through to the generic constructor path and produced nothing.
  // Same shape as the str(x) intercept above: dispatch to a
  // __pyc_tolist__ method on the argument (defined on range / list /
  // tuple / str in the builtin module).
  if (f && pos_args.n == 1 && f->name && !strcmp(f->name, "list")) {
    PycSymbol *list_cls = make_PycSymbol(ctx, "list", PYC_USE);
    if (list_cls && f == list_cls->sym) {
      PycAST *a0 = getAST(pos_args[0], ctx);
      ast->rval = new_sym(ast);
      call_method(&ast->code, ast, a0->rval, make_symbol("__pyc_tolist__"), ast->rval, 0);
      return 1;
    }
  }
  // tuple(iterable): a dynamic-length tuple cannot be a real pyc
  // tuple (fixed-arity structs -- the same constraint behind the
  // literal-only tuple +/* folding, issues/025 R1 item 4), so it
  // returns a LIST via the same __pyc_tolist__ dispatch as list().
  // Established compromise: zip/map/filter/enumerate/reversed
  // already return lists; indexing/iteration/len are identical,
  // printing/hashing differ. First users: genetic2's
  // `node.args = tuple([TreeNode() ...])`, chess's
  // `tuple(range(...))` board lines. (`tuple` resolves to the
  // ifa-core sym_tuple directly, unlike list's scoped class sym.)
  if (f == sym_tuple && pos_args.n == 1) {
    PycAST *a0 = getAST(pos_args[0], ctx);
    ast->rval = new_sym(ast);
    call_method(&ast->code, ast, a0->rval, make_symbol("__pyc_tolist__"), ast->rval, 0);
    return 1;
  }
  // issues/022: zero-arg builtin-type constructor calls (int(), float(),
  // bool(), str(), list(), tuple()) all fail identically. Root cause: the
  // generic class-instantiation lowering (clone prototype + call __init__,
  // gen_class_pyda in python_ifa_build_syms.cc) only synthesizes a __new__
  // wrapper when `is_record` -- i.e. cls->type_kind == Type_RECORD. None of
  // these six qualify: int/float are Type_ALIAS (aliased to int64/float64,
  // see python_ifa_sym.cc), bool/list/tuple are ifa-core builtin primitive
  // types (new_builtin_primitive_type, ifa/if1/ast.cc), so none of them ever
  // get a __new__ candidate to dispatch a zero-arg call to. int/float
  // additionally get a __coerce__-based 1-arg conversion path (num_kind !=
  // IF1_NUM_KIND_NONE, same file), which is why e.g. int(5) already works
  // but int() doesn't. Confirmed dict()/set() (real Type_RECORD classes
  // with an explicit __init__, see issue 017) already work fine -- this is
  // specific to non-record builtin value/container types.
  //
  // Fix: synthesize each type's zero value directly here, reusing the exact
  // codegen an equivalent literal already produces (empty `[]`/`()` use the
  // "make" primitive with no element args -- reused as-is here rather than
  // returning a shared sym_empty_list/sym_empty_tuple singleton, which would
  // alias mutations across every `list()`/`tuple()` call site, the same
  // footgun issue 017 fixed for dict/set).
  if (f && pos_args.n == 0) {
    // int/float: unlike bool/list/tuple, `atom_ast->sym` for these names
    // does not resolve to the fixed ifa-core sym_int/sym_float globals --
    // class int/float get their own Sym via normal scope resolution when
    // __pyc__/02_numeric.py's `class int:`/`class float:` are processed, so
    // these need the same by-name lookup as str, not a direct pointer
    // comparison.
    if (f->name && !strcmp(f->name, "int")) {
      PycSymbol *int_cls = make_PycSymbol(ctx, "int", PYC_USE);
      if (int_cls && f == int_cls->sym) {
        Immediate imm;
        imm.v_int64 = 0;
        ast->rval = if1_const(if1, sym_int64, "0", &imm);
        return 1;
      }
    }
    if (f->name && !strcmp(f->name, "float")) {
      PycSymbol *float_cls = make_PycSymbol(ctx, "float", PYC_USE);
      if (float_cls && f == float_cls->sym) {
        Immediate imm;
        imm.v_float64 = 0.0;
        ast->rval = if1_const(if1, sym_float64, "0", &imm);
        return 1;
      }
    }
    if (f == sym_bool) {
      ast->rval = sym_false;
      return 1;
    }
    if (f == sym_list) {
      ast->rval = new_sym(ast);
      if1_send(if1, &ast->code, 3, 1, sym_primitive, sym_make, sym_list, ast->rval)->ast = ast;
      return 1;
    }
    if (f == sym_tuple) {
      ast->rval = new_sym(ast);
      if1_send(if1, &ast->code, 3, 1, sym_primitive, sym_make, sym_tuple, ast->rval)->ast = ast;
      return 1;
    }
    if (f->name && !strcmp(f->name, "str")) {
      PycSymbol *str_cls = make_PycSymbol(ctx, "str", PYC_USE);
      if (str_cls && f == str_cls->sym) {
        ast->rval = make_string("");
        return 1;
      }
    }
  }
  if (!f || !builtin_functions.set_in(f)) return 0;
  if (f == sym_super) {
    if (!ctx.fun()) fail("super outside of function");
    PycAST *fun_ast = (PycAST *)ctx.fun()->ast;
    Vec<Sym *> fun_has;
    if (fun_ast->xpyd) {
      PyDAST *funcnode = fun_ast->xpyd;
      PyDAST *varargsl = nullptr;
      if (funcnode->kind == PY_funcdef && funcnode->children.n >= 2) {
        PyDAST *params = funcnode->children[1];
        if (params->children.n > 0) varargsl = params->children[0];
      }
      get_syms_args_pyda(fun_ast, varargsl, fun_has, ctx);
    }
    if (fun_has.n < 1 || !ctx.fun()->in || ctx.fun()->in->is_fun) fail("super outside of method");
    int n = pos_args.n;
    if (!n) {
      ast->rval = new_sym(ast);
      ast->rval->aspect = ctx.cls();
      super_aspect_syms.add(ast->rval);
      if1_move(if1, &ast->code, ctx.fun()->self, ast->rval);
    } else if (n == 1) {
      PycAST *cls_ast = getAST(pos_args[0], ctx);
      if (!cls_ast->sym || cls_ast->sym->type_kind != Type_RECORD) fail("non-constant super() class");
      ast->rval = new_sym(ast);
      ast->rval->aspect = cls_ast->sym;
      super_aspect_syms.add(ast->rval);
      if1_move(if1, &ast->code, fun_has[0], ast->rval);
    } else {
      if (n > 2) fail("bad number of arguments to builtin function 'super'");
      PycAST *a0 = getAST(pos_args[0], ctx);
      PycAST *a1 = getAST(pos_args[1], ctx);
      if (!a0->sym || a0->sym->type_kind != Type_RECORD) fail("non-constant super() class");
      ast->rval = new_sym(ast);
      ast->rval->aspect = a0->sym;
      super_aspect_syms.add(ast->rval);
      if1_move(if1, &ast->code, a1->rval, ast->rval);
    }
  } else if (f == sym___pyc_symbol__) {
    if (pos_args.n != 1) fail("bad number of arguments to builtin function %s", f->name);
    PycAST *a0 = getAST(pos_args[0], ctx);
    if (a0->rval->type != sym_string || !a0->rval->constant)
      fail("string argument required for builtin function %s", f->name);
    ast->rval = make_symbol(a0->rval->constant);
  } else if (f == sym___pyc_clone_constants__) {
    if (pos_args.n != 1) fail("bad number of arguments to builtin function %s", f->name);
    PycAST *a0 = getAST(pos_args[0], ctx);
    ast->rval = a0->rval;
    ast->rval->clone_for_constants = 1;
  } else if (f == sym___pyc_c_call__) {
    Code *send = if1_send1(if1, &ast->code, ast);
    if1_add_send_arg(if1, send, sym_primitive);
    if1_add_send_arg(if1, send, atom_ast->rval);
    for (auto a : pos_args.values()) if1_add_send_arg(if1, send, getAST(a, ctx)->rval);
    ast->rval = new_sym(ast);
    if1_add_send_result(if1, send, ast->rval);
    send->rvals.v[2]->is_fake = 1;
  } else if (f == sym___pyc_c_code__) {
    if (pos_args.n != 1) fail("bad number of arguments to builtin function %s", f->name);
    PycAST *a0 = getAST(pos_args[0], ctx);
    if (a0->rval->type != sym_string || !a0->rval->constant)
      fail("string argument required for builtin function %s", f->name);
    ctx.c_code.add(a0->rval->constant);
  } else if (f == sym___pyc_insert_c_code__ || f == sym___pyc_insert_c_header__ ||
             f == sym___pyc_include_c_header__) {
    if (pos_args.n != 1) fail("bad number of arguments to builtin function %s", f->name);
    PycAST *a0 = getAST(pos_args[0], ctx);
    if (a0->rval->type != sym_string || !a0->rval->constant)
      fail("string argument required for builtin function %s", f->name);
    cchar *file = a0->rval->constant;
    cchar *prefix = strrchr(ctx.mod->filename, '/');
    char path[PATH_MAX];
    cchar *pathname = 0;
    if (file[0] == '/' || !prefix)
      pathname = file;
    else {
      strcpy(path, ctx.mod->filename);
      strcpy(path + (prefix - ctx.mod->filename + 1), file);
      pathname = path;
    }
    if (f == sym___pyc_insert_c_code__)
      ctx.c_code.add((char *)read_file_to_string(pathname));
    else if (f == sym___pyc_insert_c_header__) {
      char code[PATH_MAX + 100];
      sprintf(code, "#include \"%s\"\n", pathname);
      ctx.c_code.add(strdup(code));
    } else {
      char cmd[PATH_MAX + 100];
      sprintf(cmd, "gcc -E %s", pathname);
      FILE *fp = popen(cmd, "r");
      if (!fp) fail("unable to include '%s'", pathname);
      pclose(fp);
      sprintf(cmd, "#include \"%s\"\n", pathname);
    }
  } else if (f == sym_print) {
    // Python 3 print() function: write each arg (space-separated) then writeln
    for (int i = 0; i < pos_args.n; i++) {
      PycAST *a = getAST(pos_args[i], ctx);
      if (i) if1_send(if1, &ast->code, 3, 1, sym_primitive, sym_write, make_string(" "), new_sym(ast))->ast = ast;
      Sym *t = new_sym(ast);
      call_method(&ast->code, ast, a->rval, sym___str__, t, 0);
      if1_send(if1, &ast->code, 3, 1, sym_primitive, sym_write, t, new_sym(ast))->ast = ast;
    }
    if1_send(if1, &ast->code, 2, 1, sym_primitive, sym_writeln, new_sym(ast))->ast = ast;
    ast->rval = sym_nil;
  } else
    fail("unimplemented builtin '%s'", f->name);
  return 1;
}

// Like call_method, but takes a runtime-determined argument list rather
// than a fixed C varargs count -- needed because a dict comprehension's
// accumulator call (__setitem__) takes two produced values (key, value)
// where a list/set comprehension's (append/add) takes one.
static void call_method_v(Code **code, PycAST *ast, Sym *o, Sym *m, Sym *r, Vec<Sym *> &args) {
  Sym *t = new_sym(ast);
  Code *method = if1_send(if1, code, 4, 1, sym_operator, o, sym_period, m, t);
  method->ast = ast;
  method->partial = Partial_OK;
  Code *send = if1_send(if1, code, 1, 1, t, r);
  send->ast = ast;
  for (Sym *v : args.values()) if1_add_send_arg(if1, send, v ? v : sym_nil);
}

// Build list/set/dict comprehension for pyda path. `elts` holds the
// expression(s) produced each iteration: one (the element) for list/set
// comprehensions, two (key, value) for dict comprehensions. `accum_method`
// is the method called on `ast->rval` (the already-created accumulator
// instance) with `elts`' values as arguments: sym_append for a list
// comprehension, "add" for a set comprehension, "__setitem__" for a dict
// comprehension. Defaults to sym_append so the existing PY_listcomp call
// site doesn't need to change.
static void build_list_comp_pyda(PyDAST *list_for, Vec<PyDAST *> &elts, PycAST *ast, Code **code, PycCompiler &ctx,
                                  Sym *accum_method = sym_append);

static void build_list_comp_inner_pyda(PyDAST *iter_node, Vec<PyDAST *> &elts, PycAST *ast, Code **code,
                                        PycCompiler &ctx, Sym *accum_method) {
  if (!iter_node) {
    // Base case: emit the produced value(s) and accumulate them.
    Vec<Sym *> args;
    for (auto elt : elts.values()) {
      build_if1_pyda(elt, ctx);
      PycAST *elt_ast = getAST(elt, ctx);
      if1_gen(if1, code, elt_ast->code);
      args.add(elt_ast->rval);
    }
    Sym *new_val = new_sym(ast);
    call_method_v(code, ast, ast->rval, accum_method, new_val, args);
    if1_move(if1, code, new_val, ast->rval, ast);
    return;
  }
  if (iter_node->kind == PY_list_for || iter_node->kind == PY_comp_for) {
    build_list_comp_pyda(iter_node, elts, ast, code, ctx, accum_method);
  } else if (iter_node->kind == PY_list_if || iter_node->kind == PY_comp_if) {
    // PY_list_if / PY_comp_if: children = [test, list_iter?]
    build_if1_pyda(iter_node->children[0], ctx);
    PycAST *test_ast = getAST(iter_node->children[0], ctx);
    if1_gen(if1, code, test_ast->code);
    Label *short_circuit = if1_alloc_label(if1);
    Code *ifcode = if1_if_goto(if1, code, test_ast->rval, ast);
    if1_if_label_false(if1, ifcode, short_circuit);
    if1_if_label_true(if1, ifcode, if1_label(if1, code, ast));
    PyDAST *next_iter = (iter_node->children.n > 1) ? iter_node->children[1] : nullptr;
    build_list_comp_inner_pyda(next_iter, elts, ast, code, ctx, accum_method);
    if1_label(if1, code, ast, short_circuit);
  }
}

static void build_list_comp_pyda(PyDAST *list_for, Vec<PyDAST *> &elts, PycAST *ast, Code **code, PycCompiler &ctx,
                                  Sym *accum_method) {
  // list_for/comp_for: children = [target_exprlist, iter_testlist, list_iter?]
  PyDAST *target = list_for->children[0];
  PyDAST *iter_expr = list_for->children[1];
  PyDAST *next_iter = (list_for->children.n > 2) ? list_for->children[2] : nullptr;
  // Get target AST from build_syms
  PycAST *t = getAST(target, ctx);
  build_if1_pyda(iter_expr, ctx);
  PycAST *i_ast = getAST(iter_expr, ctx);
  Code *before = 0, *cond = 0, *body = 0, *next = 0;
  Sym *iter = new_sym(ast), *cond_var = new_sym(ast), *tmp = new_sym(ast);
  if1_gen(if1, &before, i_ast->code);
  if1_send(if1, &before, 2, 1, sym___iter__, i_ast->rval, iter)->ast = ast;
  call_method(&cond, ast, iter, sym___pyc_more__, cond_var, 0);
  call_method(&body, ast, iter, sym___next__, tmp, 0);
  // General target path: handles `[... for (i, c) in zip(...)]`
  // tuple unpacking, same as PY_for_stmt (issue 025 "has no type"
  // bucket -- collatz line 39). The old raw move into t->sym was
  // null for tuple targets; the old `if1_gen(t->code)` for a tuple
  // target emitted a spurious make-tuple over unbound names (the
  // non-tuple branch of emit_assign_to_target still gens a->code).
  (void)t;
  emit_assign_to_target(target, tmp, &body, ast, ctx);
  build_list_comp_inner_pyda(next_iter, elts, ast, &body, ctx, accum_method);
  if1_loop(if1, code, if1_alloc_label(if1), if1_alloc_label(if1), cond_var, before, cond, next, body, ast);
}

// Helper: build IF1 code for a suite-or-statement child.
// The grammar's 'suite' alternative may be a PY_suite node (multi-statement)
// or a direct statement node (single-statement, e.g. "if x: break").
static void build_if1_suite_pyda(PyDAST *node, Code **code, PycCompiler &ctx) {
  if (node->kind == PY_suite) {
    for (auto c : node->children.values()) {
      build_if1_pyda(c, ctx);
      if1_gen(if1, code, getAST(c, ctx)->code);
    }
  } else {
    build_if1_pyda(node, ctx);
    if1_gen(if1, code, getAST(node, ctx)->code);
  }
}

// Re-enter function scope (used in build_if1 pass for funcdef/classdef)
static void reenter_scope_pyda(PyDAST *n, PycCompiler &ctx) {
  ctx.node = n;
  PycScope *c = ctx.saved_scopes.get(n);
  assert(c);
  ctx.scope_stack.add(c);
  TEST_SCOPE printf("enter scope level %d\n", ctx.scope_stack.n);
}

// Helper: build if-else chain for PY_if_stmt
static void build_if_pyda(PyDAST *n, PycAST *ast, PycCompiler &ctx) {
  // n is PY_if_stmt: children = [cond, suite, elif1, elif2, ..., else?]
  // Build elif/else chain inside-out: process from innermost (last) to outermost.
  // This ensures each elif's internal goto jumps past the entire remaining chain.

  // Find else_clause (last child if it's PY_else_clause)
  int last = n->children.n - 1;
  Code *chain = 0;  // the accumulated else chain (starts with innermost else)
  if (last >= 2 && n->children[last]->kind == PY_else_clause) {
    build_if1_suite_pyda(n->children[last]->children[0], &chain, ctx);
    last--;
  }
  // Wrap elifs around chain from innermost to outermost
  for (int i = last; i >= 2; i--) {
    PyDAST *child = n->children[i];
    if (child->kind != PY_elif_clause) continue;
    PycAST *elif_ast = getAST(child, ctx);
    PyDAST *elif_cond = child->children[0];
    PyDAST *elif_suite = child->children[1];
    build_if1_pyda(elif_cond, ctx);
    PycAST *elif_cond_ast = getAST(elif_cond, ctx);
    Sym *elif_t = new_sym(elif_ast);
    Code *elif_chain = 0;
    if1_gen(if1, &elif_chain, elif_cond_ast->code);
    call_method(&elif_chain, elif_ast, elif_cond_ast->rval, sym___pyc_to_bool__, elif_t, 0);
    Code *elif_then = 0;
    build_if1_suite_pyda(elif_suite, &elif_then, ctx);
    if1_if(if1, &elif_chain, 0, elif_t, elif_then, 0, chain, 0, 0, elif_ast);
    chain = elif_chain;
  }
  // Build outer if
  PyDAST *cond = n->children[0];
  PyDAST *suite = n->children[1];
  build_if1_pyda(cond, ctx);
  PycAST *cond_ast = getAST(cond, ctx);
  if1_gen(if1, &ast->code, cond_ast->code);
  Sym *t = new_sym(ast);
  call_method(&ast->code, ast, cond_ast->rval, sym___pyc_to_bool__, t, 0);
  Code *then_code = 0;
  build_if1_suite_pyda(suite, &then_code, ctx);
  if1_if(if1, &ast->code, 0, t, then_code, 0, chain, 0, 0, ast);
}

// Flatten a left-folded PY_binop('|') tree (python.g's
// build_binop_list produces `((1 | 2) | 3)` for `1 | 2 | 3`) into
// its leaf patterns, in source (left-to-right) order.
static void flatten_or_pattern(PyDAST *n, Vec<PyDAST *> &out) {
  if (n->kind == PY_binop && n->op == PY_OP_BITOR) {
    flatten_or_pattern(n->children[0], out);
    flatten_or_pattern(n->children[1], out);
  } else {
    out.add(n);
  }
}

// A case_block's optional guard (`case PATTERN if test:`) shows up
// as a PY_case_guard child somewhere among CASE_KW/pattern/suite --
// found by kind, not position, since this codebase's PyDAST child
// indexing for case_block is already order-fragile (see the
// children.n == 3 / >= 4 / else dance in build_match_pyda) and a
// guard shifts child counts around further.
static PyDAST *find_case_guard(PyDAST *case_block) {
  for (auto c : case_block->children.values())
    if (c && c->kind == PY_case_guard) return c;
  return nullptr;
}

// Evaluate a case_guard's `if test` condition (python.g:
// `case_guard: 'if' test`) into a boolean Sym, appending its code
// to *code. Returns nullptr if there's no guard, so callers can
// treat "no guard" and "guard present" uniformly by just checking
// the return value.
static Sym *eval_case_guard(PyDAST *guard, Code **code, PycAST *case_ast, PycCompiler &ctx) {
  if (!guard || !guard->children.n) return nullptr;
  PyDAST *cond = guard->children[guard->children.n - 1];
  build_if1_pyda(cond, ctx);
  PycAST *cond_ast = getAST(cond, ctx);
  if1_gen(if1, code, cond_ast->code);
  Sym *guard_bool = new_sym(case_ast);
  call_method(code, case_ast, cond_ast->rval, sym___pyc_to_bool__, guard_bool, 0);
  return guard_bool;
}

// Classify a PY_number pattern node as int or float, mirroring
// make_num_pyda's own classification exactly (duplicated rather than
// factored out: make_num_pyda returns an already-built Sym, not a
// yes/no answer, and this needs the answer before deciding whether to
// build an isinstance narrowing around the literal-pattern comparison
// at all). A literal pattern needs to know which builtin class
// ("int" vs "float") to narrow the subject to -- see the literal
// fallback's comment in build_pattern_match.
static bool number_pattern_is_float(PyDAST *n) {
  if (n->is_int) return false;
  cchar *s = n->str_val ? n->str_val : "0";
  for (cchar *p = s; *p; p++)
    if (*p == '.' || *p == 'e' || *p == 'E' || *p == 'j' || *p == 'J') return true;
  return false;
}

// Resolve a global name (a builtin class like "list"/"tuple") the
// same way an ordinary PY_name reference to it would -- via the
// normal scope-lookup machinery (find_PycSymbol, via
// make_PycSymbol's PYC_USE path), just invoked directly by string
// instead of parsing a PY_name node. Needed by sequence-pattern
// matching, which has to reference `list`'s class Sym for an
// isinstance check with no corresponding source-text token to hang
// a real AST node off of.
static Sym *resolve_global_class(cchar *name, PycCompiler &ctx) {
  PycSymbol *s = make_PycSymbol(ctx, name, PYC_USE);
  if (!s) fail("error line %d: cannot resolve builtin '%s' needed for pattern matching", ctx.lineno, name);
  return s->sym;
}

// Call the real `isinstance(obj, cls)` builtin (__pyc__/05_builtins.py)
// as an ordinary plain function call -- if1_send1 + if1_add_send_arg,
// the same shape build_if1_pyda's own PY_call trailer uses for any
// non-method callee. Deliberately NOT the raw `sym_primitive`
// isinstance send `x is None` uses (python_ifa_build_if1.cc's
// PY_compare handling): that form is tied to FA's isinstance-based
// type-narrowing, which expects the send to sit directly in an
// if-condition position. Sequence-pattern matching needs to combine
// several isinstance/length/element checks with bool.__and__/__or__
// before the actual if1_if, and the raw primitive form breaks
// (confirmed empirically -- "illegal call argument" FA errors) once
// composed that way. The ordinary-call form is exactly what a real
// `isinstance(x, list) or isinstance(x, tuple)` in user source
// already compiles to, so it's proven to compose correctly.
static Sym *build_isinstance_call(Sym *obj, Sym *cls, Code **code, PycAST *case_ast, PycCompiler &ctx) {
  Sym *iso_fn = resolve_global_class("isinstance", ctx);
  Code *send = if1_send1(if1, code, case_ast);
  if1_add_send_arg(if1, send, iso_fn);
  if1_add_send_arg(if1, send, obj);
  if1_add_send_arg(if1, send, cls);
  Sym *result = new_sym(case_ast);
  if1_add_send_result(if1, send, result);
  return result;
}

// Read `obj.attr`'s value -- the exact "period" operator send
// PY_power's own PY_attribute trailer handling emits when flushing a
// pending member access (see build_if1_pyda's PY_power case), just
// synthesized directly by string instead of walking a real
// PY_attribute AST node. Needed by class-pattern matching to read
// each keyword sub-pattern's target attribute (`Point(x=0, y=0)`
// reads `.x` and `.y` off the subject) with no source-text token to
// hang a real AST node off of -- same rationale as
// resolve_global_class/build_isinstance_call above.
static Sym *build_attribute_get(Sym *obj, cchar *attr, Code **code, PycAST *case_ast) {
  Sym *result = new_sym(case_ast);
  Code *mem = if1_send(if1, code, 4, 1, sym_operator, obj, sym_period, make_symbol(attr), result);
  mem->ast = case_ast;
  mem->partial = Partial_OK;
  return result;
}

// Build `if cond: result = build_then(&then_code) else: result =
// sym_false`, appending the whole if1_if into *code and returning
// the fresh `result` Sym.
//
// This is NOT the same as combine_bool's flat AND-fold, and the
// difference matters: FA analyzes the WHOLE program statically, not
// per-runtime-branch. A check that's unconditionally *emitted* --
// even if its result only ever gets AND-folded and effectively
// ignored once a prior check already failed -- still has to
// type-check against the subject's full, unnarrowed static type.
// Concretely: if `subject`'s type at this match arm is a union like
// `list | int` (because some OTHER case in the same match statement
// only matches when subject is an int), unconditionally emitting
// `subject.__len__()` fails FA type checking for the int member,
// even though that call's result is only ever ANDed into a boolean
// that's never read when the preceding isinstance check is false.
// Gating it behind a REAL if1_if narrows `subject`'s type within
// the "then" branch (the same narrowing mechanism `x is None`
// relies on -- see resolve_global_class's comment), so __len__/
// __getitem__ only ever need to type-check against the narrowed
// list/tuple type, not the full union. Used for every structural
// pattern (sequence pattern today; mapping/class patterns will need
// the identical shape).
static Sym *guarded_bool(Sym *cond, Code **code, PycAST *case_ast,
                          const std::function<Sym *(Code **)> &build_then) {
  Sym *result = new_sym(case_ast);
  Code *then_code = 0;
  Sym *then_val = build_then(&then_code);
  if1_move(if1, &then_code, then_val, result, case_ast);
  Code *else_code = 0;
  if1_move(if1, &else_code, sym_false, result, case_ast);
  if1_if(if1, code, 0, cond, then_code, 0, else_code, 0, 0, case_ast);
  return result;
}

// Combine two boolean Syms with a bool method (__and__/__or__),
// producing a fresh result Sym. Small helper since build_pattern_match
// folds several of these per pattern kind.
//
// `sym_true` (the canonical always-matches sentinel build_pattern_match
// returns for wildcard/capture patterns, matching build_match_pyda's
// own pointer-comparison short-circuit) is NOT a properly-typed
// runtime bool instance -- it's a compile-time marker. Calling
// .__and__/.__or__ ON it directly fails FA type checking ("illegal
// call argument"). Short-circuit around it algebraically instead
// (X and True == X; X or True == True) so it's never used as a
// method-dispatch receiver or argument -- this is exactly why
// build_pattern_match returns sym_true in the first place, so every
// caller that folds multiple sub-results together needs to respect
// it, not just the top-level build_match_pyda caller.
static Sym *combine_bool(Sym *a, Sym *b, cchar *method_name, Code **code, PycAST *case_ast) {
  bool is_and = !strcmp(method_name, "__and__");
  if (a == sym_true) return is_and ? b : sym_true;
  if (b == sym_true) return is_and ? a : sym_true;
  Sym *r = new_sym(case_ast);
  call_method(code, case_ast, a, make_symbol(method_name), r, 1, b);
  return r;
}

// -------------------------------------------------------------
// Recursive pattern matcher (PEP 634's match/case).
//
// Returns a boolean Sym: does `pattern` match `subject`? Any
// bindings the pattern introduces (captures; sequence-pattern
// element captures) are emitted as MOVEs into *code as a side
// effect, targeting whatever PYC_LOCAL symbol build_syms_pyda's
// mark_pattern_captures already created for each bare name in the
// pattern (see PY_case_block's symbol-building case).
//
// The canonical `sym_true` Sym is returned directly (not a freshly
// computed boolean) for wildcard and capture patterns, so callers
// can special-case "irrefutable, no check needed at all" by
// pointer-comparing the result against sym_true rather than
// emitting a redundant `if1_if` on a value that's always true.
//
// Every pattern kind this function doesn't recognize as a
// structural pattern (sequence: PY_list/PY_tuple; or-pattern:
// PY_binop '|') falls through to literal-pattern handling: build it
// as an ordinary expression and compare via __eq__ -- PEP 634's own
// rule for literal/value patterns.
//
// `guard_eval`, when non-null, is the case's `if cond:` guard
// (already bound to a specific case_block by the caller), evaluated
// via the callback exactly once -- at the point THIS call
// establishes its own match, in whatever Code* scope that happens
// to be. This matters because sequence patterns build genuine
// nested if1_if control flow (see guarded_bool below): their
// element bindings only exist along the path where isinstance/
// length checks already passed, so a guard referencing those
// bindings (`case [a, b] if a > b:`) must be evaluated INSIDE that
// same nested "then" branch, not appended afterward in the flat
// outer code stream where FA can't prove `a`/`b` are defined.
// Recursive calls (or-pattern alternatives, sequence-pattern
// elements) always pass guard_eval=nullptr -- the guard applies
// once to the whole top-level pattern, not per sub-pattern.
// -------------------------------------------------------------
static Sym *build_pattern_match(PyDAST *pattern, Sym *subject, Code **code, PycAST *case_ast, PycCompiler &ctx,
                                 const std::function<Sym *(Code **)> *guard_eval = nullptr) {
  if (pattern->kind == PY_name && !strcmp(pattern->str_val, "_")) {
    // Wildcard: always matches, no binding -- the guard alone decides.
    return guard_eval ? (*guard_eval)(code) : sym_true;
  }
  if (pattern->kind == PY_name && strcmp(pattern->str_val, "None") && strcmp(pattern->str_val, "True") &&
      strcmp(pattern->str_val, "False")) {
    // Capture pattern: always matches (irrefutable), binds subject.
    // build_syms_pyda's mark_pattern_captures already marked this
    // node PY_STORE, so getAST(pattern)->sym is a freshly created
    // PYC_LOCAL rather than an existing binding -- move the subject
    // into it, same shape as a plain `x = subject` assignment (see
    // emit_assign_to_target's simple-name branch).
    //
    // `None`/`True`/`False` are excluded here even though they parse
    // as bare PY_name too (they're ordinary global constants, not
    // syntax keywords in this grammar) -- PEP 634 treats them as
    // literal/singleton patterns, not captures: `case None:` must
    // only match when the subject actually IS None, not bind a new
    // local named "None" and match unconditionally. Falls through to
    // the literal-pattern handling below.
    build_if1_pyda(pattern, ctx);
    PycAST *pat_ast = getAST(pattern, ctx);
    if1_gen(if1, code, pat_ast->code);
    if1_move(if1, code, subject, pat_ast->sym, case_ast);
    return guard_eval ? (*guard_eval)(code) : sym_true;
  }
  if (pattern->kind == PY_binop && pattern->op == PY_OP_BITOR) {
    // Or-pattern (PEP 634): `1 | 2 | 3` matches if the subject
    // matches ANY alternative. Without this, the pattern parses as
    // an ordinary expression -- `1 | 2` evaluates to the integer 3
    // via bitwise-OR, then gets compared against the subject via
    // __eq__ like a single literal ("match if subject equals 3,"
    // not "match if subject equals 1 or 2") -- a silent wrong-
    // answer miscompile, not a crash or a diagnostic.
    Vec<PyDAST *> alts;
    flatten_or_pattern(pattern, alts);
    Sym *combined = nullptr;
    for (int j = 0; j < alts.n; j++) {
      PyDAST *alt = alts.v[j];
      if (alt->kind == PY_name)
        fail("error line %d: or-pattern alternative '%s' is a capture/wildcard pattern -- "
             "only literal alternatives are supported in 'case ... | ...:'",
             ctx.lineno, alt->str_val);
      Sym *alt_matched = build_pattern_match(alt, subject, code, case_ast, ctx);
      combined = combined ? combine_bool(combined, alt_matched, "__or__", code, case_ast) : alt_matched;
    }
    // Guard applies once, to the whole or-pattern's match result --
    // not per-alternative (see build_pattern_match's guard_eval doc).
    if (guard_eval) combined = combine_bool(combined, (*guard_eval)(code), "__and__", code, case_ast);
    return combined;
  }
  if (pattern->kind == PY_list || pattern->kind == PY_tuple) {
    // Sequence pattern (PEP 634): `[a, b]` / `(a, b)` matches any
    // list or tuple of the same length, binding each element to its
    // sub-pattern (which may itself be any pattern kind, including
    // a nested sequence pattern -- this recurses).
    //
    // Star patterns (`case [a, *rest]:`) are not yet supported --
    // same underlying grammar gap as issue 024's `a, *b = ...`
    // extended-unpacking assignment targets, which also doesn't
    // parse today. Left for a follow-on.
    int n_elts = pattern->children.n;

    // isinstance(subject, list) or isinstance(subject, tuple) --
    // sequence patterns match list/tuple-like values, NOT str/bytes
    // (which also support __len__/__getitem__ but PEP 634
    // explicitly excludes them: `case [a, b]:` must not match the
    // 2-character string "hi").
    Sym *list_cls = resolve_global_class("list", ctx);
    Sym *tuple_cls = resolve_global_class("tuple", ctx);
    Sym *is_list = build_isinstance_call(subject, list_cls, code, case_ast, ctx);
    Sym *is_tuple = build_isinstance_call(subject, tuple_cls, code, case_ast, ctx);
    Sym *is_seq = combine_bool(is_list, is_tuple, "__or__", code, case_ast);

    // The length check and per-element __getitem__/recursive match
    // all go inside guarded_bool's "then" branch: subject's type is
    // only narrowed to list/tuple WITHIN a branch actually gated on
    // is_seq (a real if1_if, not a flat AND-fold) -- see
    // guarded_bool's comment for why that distinction is required,
    // not just a style preference. isinstance() itself doesn't need
    // this gating (it's polymorphic over any subject type, unlike
    // __len__/__getitem__), so is_list/is_tuple/is_seq above stay
    // unconditional.
    return guarded_bool(is_seq, code, case_ast, [&](Code **then_code) -> Sym * {
      // len(subject) == n_elts -- via __len__ directly (mirrors
      // __pyc__/05_builtins.py's `def len(x): return x.__len__()`)
      // rather than resolving "len" as a free-function reference.
      Sym *len_result = new_sym(case_ast);
      call_method(then_code, case_ast, subject, make_symbol("__len__"), len_result, 0);
      Sym *len_eq = new_sym(case_ast);
      call_method(then_code, case_ast, len_result, sym___eq__, len_eq, 1, int64_constant(n_elts));
      Sym *len_bool = new_sym(case_ast);
      call_method(then_code, case_ast, len_eq, sym___pyc_to_bool__, len_bool, 0);

      // Per-element recursive match, ALSO nested inside its own
      // guarded_bool -- subject[j] must not be emitted unconditionally
      // either, for the identical reason the length check needed
      // gating (an element sub-pattern can itself be a further
      // sequence pattern needing its own subject to be narrowed).
      return guarded_bool(len_bool, then_code, case_ast, [&](Code **elems_code) -> Sym * {
        Sym *combined = nullptr;
        for (int j = 0; j < n_elts; j++) {
          PyDAST *elt = pattern->children[j];
          Sym *item = new_sym(case_ast);
          call_method(elems_code, case_ast, subject, sym___getitem__, item, 1, int64_constant(j));
          Sym *elt_matched = build_pattern_match(elt, item, elems_code, case_ast, ctx);
          combined = combined ? combine_bool(combined, elt_matched, "__and__", elems_code, case_ast) : elt_matched;
        }
        Sym *result = combined ? combined : sym_true; // `case []:` -- zero elements, nothing to check
        // Guard evaluated HERE, inside the innermost "then" branch --
        // this is the only point where every element binding this
        // pattern makes is actually established, which is exactly
        // where a guard referencing them (`case [a, b] if a > b:`)
        // needs to run. See build_pattern_match's guard_eval doc.
        if (guard_eval) {
          Sym *g = (*guard_eval)(elems_code);
          result = (result == sym_true) ? g : combine_bool(result, g, "__and__", elems_code, case_ast);
        }
        return result;
      });
    });
  }
  if (pattern->kind == PY_dict) {
    // Mapping pattern (PEP 634): `{"k1": p1, "k2": p2}` matches any
    // mapping containing every given key, recursively matching each
    // key's value against its sub-pattern. Keys are ordinary VALUE
    // expressions (literals, or a value pattern like `Color.RED`),
    // evaluated once up front -- not sub-patterns themselves
    // (mark_pattern_captures deliberately leaves the key side
    // untouched, mirroring PEP 634's own rule that mapping-pattern
    // keys must be literals/value patterns, never captures).
    //
    // `**rest` (PEP 634's rest-of-mapping capture) isn't supported:
    // python.g's dictorsetmaker has no `'**' NAME` alternative at all
    // (real Python's dict-merge literal, `{**other}`, isn't a pyc
    // feature either), so it fails to parse with an ordinary syntax
    // error rather than reaching this code -- same "doesn't parse,
    // so at least loud" deferral as sequence patterns' `*rest`.
    int n_pairs = pattern->children.n / 2;
    Vec<Sym *> keys;
    for (int j = 0; j < n_pairs; j++) {
      PyDAST *key = pattern->children[j * 2];
      build_if1_pyda(key, ctx);
      PycAST *key_ast = getAST(key, ctx);
      if1_gen(if1, code, key_ast->code);
      keys.add(key_ast->rval);
    }
    Sym *dict_cls = resolve_global_class("dict", ctx);
    Sym *is_map = build_isinstance_call(subject, dict_cls, code, case_ast, ctx);

    // Same two-level guarded_bool nesting as sequence patterns, and
    // for the identical reason (see guarded_bool's comment):
    // subject's type is only narrowed to dict WITHIN the outer
    // "then" branch, and value retrieval must not run unless every
    // key is confirmed present first (a missing key's __getitem__
    // would be a runtime KeyError in the underlying dict, not
    // "doesn't match").
    return guarded_bool(is_map, code, case_ast, [&](Code **then_code) -> Sym * {
      if (n_pairs == 0) {
        // `case {}:` -- no keys to check, matches any mapping.
        return guard_eval ? (*guard_eval)(then_code) : sym_true;
      }
      Sym *all_present = nullptr;
      for (int j = 0; j < n_pairs; j++) {
        Sym *has = new_sym(case_ast);
        call_method(then_code, case_ast, subject, make_symbol("__contains__"), has, 1, keys[j]);
        all_present = all_present ? combine_bool(all_present, has, "__and__", then_code, case_ast) : has;
      }
      return guarded_bool(all_present, then_code, case_ast, [&](Code **vals_code) -> Sym * {
        Sym *combined = nullptr;
        for (int j = 0; j < n_pairs; j++) {
          PyDAST *val_pat = pattern->children[j * 2 + 1];
          Sym *item = new_sym(case_ast);
          call_method(vals_code, case_ast, subject, sym___getitem__, item, 1, keys[j]);
          Sym *val_matched = build_pattern_match(val_pat, item, vals_code, case_ast, ctx);
          combined = combined ? combine_bool(combined, val_matched, "__and__", vals_code, case_ast) : val_matched;
        }
        Sym *result = combined ? combined : sym_true;
        if (guard_eval) {
          Sym *g = (*guard_eval)(vals_code);
          result = (result == sym_true) ? g : combine_bool(result, g, "__and__", vals_code, case_ast);
        }
        return result;
      });
    });
  }
  if (pattern->kind == PY_power && pattern->children.n == 2 && pattern->children[0]->kind == PY_name &&
      pattern->children[1]->kind == PY_call) {
    // Class pattern (PEP 634): `Point(x=0, y=0)` matches when the
    // subject isinstance()s the named class AND every keyword
    // sub-pattern matches the corresponding attribute's value.
    // Parses via the ordinary constructor-call grammar (same trick
    // sequence patterns use with list/tuple literals) -- with no
    // dedicated pattern grammar, `Point(x=0, y=0)` is indistinguishable
    // at parse time from an actual constructor call; recognizing it
    // here, before it reaches the literal-pattern fallback below
    // (which would build it as a real expression and construct an
    // actual Point instance to compare via __eq__), is what makes it
    // a pattern instead of the same silent-miscompile trap or-patterns
    // and sequence patterns hit before their own fixes.
    //
    // Positional class patterns (`Point(0, 0)`, matched via
    // `__match_args__`) are NOT supported -- pyc has no compile-time
    // read-back of a class-body literal assignment like
    // `__match_args__ = ("x", "y")`, unlike sequence patterns which
    // could reuse existing `__getitem__`/`__len__` machinery wholesale.
    // Fails loudly rather than guessing; keyword-only class patterns
    // are PEP 634's more common, more explicit form regardless.
    cchar *cls_name = pattern->children[0]->str_val;
    PyDAST *call = pattern->children[1];
    Vec<PyDAST *> kw_names, kw_pats;
    if (call->children.n > 0) {
      PyDAST *arglist = call->children[0];
      for (auto arg : arglist->children.values()) {
        if (arg->kind != PY_keyword_arg)
          fail("error line %d: positional class pattern arguments ('%s(...)' without "
               "'attr=pattern' keyword names) are not yet supported -- use "
               "'%s(attr=pattern, ...)'",
               ctx.lineno, cls_name, cls_name);
        kw_names.add(arg->children[0]);
        kw_pats.add(arg->children[1]);
      }
    }
    Sym *cls = resolve_global_class(cls_name, ctx);
    Sym *is_inst = build_isinstance_call(subject, cls, code, case_ast, ctx);

    // Single-level guarded_bool: attribute reads (unlike sequence
    // patterns' length-then-elements) have no second gating condition
    // of their own -- the isinstance narrowing this branch already
    // provides is all FA needs before type-checking `.attr` accesses
    // against the narrowed class type.
    return guarded_bool(is_inst, code, case_ast, [&](Code **then_code) -> Sym * {
      Sym *combined = nullptr;
      for (int j = 0; j < kw_names.n; j++) {
        Sym *val = build_attribute_get(subject, kw_names[j]->str_val, then_code, case_ast);
        Sym *sub_matched = build_pattern_match(kw_pats[j], val, then_code, case_ast, ctx);
        combined = combined ? combine_bool(combined, sub_matched, "__and__", then_code, case_ast) : sub_matched;
      }
      Sym *result = combined ? combined : sym_true;
      if (guard_eval) {
        Sym *g = (*guard_eval)(then_code);
        result = (result == sym_true) ? g : combine_bool(result, g, "__and__", then_code, case_ast);
      }
      return result;
    });
  }
  if (pattern->kind == PY_name && !strcmp(pattern->str_val, "None")) {
    // `None`/`True`/`False` singleton patterns (PEP 634) match by
    // identity, not equality -- `case None:` must not match on some
    // OTHER falsy-but-not-None value, and (True/False, handled
    // separately below) `1 == True` in real Python must not make
    // `case True:` match an int.
    //
    // NOT `subject.__null__()`: a method dispatch on a union
    // receiver, confirmed empirically to crash at RUNTIME ("matching
    // function not found") when combined with a later
    // isinstance-narrowed pattern (`case 5:`, `case [a, b]:`, ...) in
    // the same match -- codegen never generated a clone for one of
    // the union's actually-reachable runtime types. Using the raw
    // `prim_isinstance` primitive directly (the exact form the
    // pre-existing `x is None` expression lowering just above this
    // function already uses, specifically BECAUSE method dispatch
    // doesn't split/narrow correctly for union receivers) does NOT
    // fix this -- confirmed empirically to fail the identical way.
    // This appears to be a deeper FA/codegen clone-generation gap
    // specific to combining a NoneType-narrowing branch with a
    // *subsequent* isinstance-narrowing branch for the same subject
    // in a single function, not something fixable from this lowering
    // code. build_match_pyda refuses to compile a match statement
    // that combines `case None:` with any OTHER isinstance-narrowed
    // pattern, rather than silently emitting code that can crash --
    // see that check for the full explanation. This branch is only
    // reachable when `case None:` is the ONLY narrowed pattern kind
    // in the match (safe: `isinstance(subject, sym_nil_type)` alone,
    // or combined only with wildcard/capture/None arms, compiles and
    // runs correctly).
    Sym *is_none = build_isinstance_call(subject, sym_nil_type, code, case_ast, ctx);
    return guarded_bool(is_none, code, case_ast,
                         [&](Code **then_code) -> Sym * { return guard_eval ? (*guard_eval)(then_code) : sym_true; });
  }
  if (pattern->kind == PY_name && (!strcmp(pattern->str_val, "True") || !strcmp(pattern->str_val, "False"))) {
    // `True`/`False` singleton patterns: NOT built via the general
    // literal path below, because that compares against
    // `pat_ast->rval` -- for `True`/`False` that would be the raw
    // `sym_true`/`sym_false` sentinel, which (per combine_bool's own
    // comment, hit earlier landing sequence patterns) is a
    // compile-time marker, not a properly-typed runtime bool
    // instance, and fails FA type checking when passed as a method
    // argument the same way it does as a method receiver. Sidestep
    // entirely: once narrowed to `bool`, `case True:` is just
    // "subject is truthy" and `case False:` is "subject is falsy" --
    // no comparison against the sentinel needed at all.
    //
    bool want_true = !strcmp(pattern->str_val, "True");
    Sym *bool_cls = resolve_global_class("bool", ctx);
    Sym *is_bool = build_isinstance_call(subject, bool_cls, code, case_ast, ctx);
    return guarded_bool(is_bool, code, case_ast, [&](Code **then_code) -> Sym * {
      Sym *truthy = new_sym(case_ast);
      call_method(then_code, case_ast, subject, sym___pyc_to_bool__, truthy, 0);
      Sym *bool_result = truthy;
      if (!want_true) {
        bool_result = new_sym(case_ast);
        call_method(then_code, case_ast, truthy, make_symbol("__not__"), bool_result, 0);
      }
      if (guard_eval) {
        Sym *g = (*guard_eval)(then_code);
        bool_result = combine_bool(bool_result, g, "__and__", then_code, case_ast);
      }
      return bool_result;
    });
  }
  // Literal pattern: build it as an ordinary expression, compare via
  // __eq__. Narrowed via isinstance first when the literal's own
  // class is staticaly known (number/string) -- for the same reason
  // sequence/mapping/class patterns need guarded_bool: FA type-checks
  // the whole program statically, so an unconditional
  // subject.__eq__(literal) has to type-check against subject's FULL
  // static union (every OTHER case arm's type included, since they
  // all flow into the same match subject) -- and cross-type __eq__
  // dispatch (comparing an int-typed union member against a string
  // literal, say) doesn't codegen correctly today. A match statement
  // mixing literal types across arms (`case 1: ... case "a": ...`)
  // hit exactly this before the narrowing was added. Patterns this
  // can't classify (dotted value patterns like `Color.RED`) fall
  // back to the old unconditional form -- narrower coverage than
  // number/string, but no worse than before this fix.
  build_if1_pyda(pattern, ctx);
  PycAST *pat_ast = getAST(pattern, ctx);
  if1_gen(if1, code, pat_ast->code);
  cchar *lit_cls_name = nullptr;
  if (pattern->kind == PY_string) lit_cls_name = "str";
  else if (pattern->kind == PY_number) lit_cls_name = number_pattern_is_float(pattern) ? "float" : "int";
  auto do_compare = [&](Code **cmp_code) -> Sym * {
    Sym *cmp_result = new_sym(case_ast);
    Sym *bool_result = new_sym(case_ast);
    call_method(cmp_code, case_ast, subject, sym___eq__, cmp_result, 1, pat_ast->rval);
    call_method(cmp_code, case_ast, cmp_result, sym___pyc_to_bool__, bool_result, 0);
    if (guard_eval) bool_result = combine_bool(bool_result, (*guard_eval)(cmp_code), "__and__", cmp_code, case_ast);
    return bool_result;
  };
  if (!lit_cls_name) return do_compare(code);
  Sym *lit_cls = resolve_global_class(lit_cls_name, ctx);
  Sym *is_same_type = build_isinstance_call(subject, lit_cls, code, case_ast, ctx);
  return guarded_bool(is_same_type, code, case_ast, do_compare);
}

// Does `pattern` (or anything nested inside it -- a sequence
// element, a mapping value, a class pattern's keyword value, an
// or-pattern alternative) contain a `None` singleton pattern?
// Recurses through every pattern kind build_pattern_match itself
// recurses through, for the same reason build_match_pyda's None+
// isinstance-narrowing check below needs it: a nested `case [None,
// x]:` still puts a None-narrowing branch and an isinstance-
// narrowing branch (the sequence pattern's own isinstance(subject,
// list) check) in the same function, which is the exact shape
// confirmed to crash at runtime -- see that check's comment.
static bool pattern_contains_none(PyDAST *pattern) {
  if (!pattern) return false;
  if (pattern->kind == PY_name) return !strcmp(pattern->str_val, "None");
  if (pattern->kind == PY_binop && pattern->op == PY_OP_BITOR)
    return pattern_contains_none(pattern->children[0]) || pattern_contains_none(pattern->children[1]);
  if (pattern->kind == PY_list || pattern->kind == PY_tuple) {
    for (auto c : pattern->children.values())
      if (pattern_contains_none(c)) return true;
    return false;
  }
  if (pattern->kind == PY_dict) {
    for (int i = 1; i < pattern->children.n; i += 2)
      if (pattern_contains_none(pattern->children[i])) return true;
    return false;
  }
  if (pattern->kind == PY_power && pattern->children.n == 2 && pattern->children[1]->kind == PY_call) {
    PyDAST *call = pattern->children[1];
    if (call->children.n > 0)
      for (auto arg : call->children[0]->children.values())
        if (arg->kind == PY_keyword_arg && pattern_contains_none(arg->children[1])) return true;
    return false;
  }
  return false;
}

// Does `pattern` (or anything nested inside it) do anything besides
// an unconditional, no-op wildcard match -- specifically: does it
// require isinstance-based narrowing (True/False, number/string
// literals, or a structural sequence/mapping/class pattern -- every
// kind build_pattern_match gates behind guarded_bool/
// build_isinstance_call), OR does it bind the subject into a fresh
// local at all (a bare capture, or ANY structural pattern's element/
// value/attribute sub-bindings)? Both are confirmed unsafe combined
// with a `case None:` elsewhere in the same match -- see the
// has_none/has_risky check below for what "unsafe" means concretely.
// Only wildcard (`_`) and None itself come back false: combining
// `case None:` with ONLY wildcard and/or other None arms is
// confirmed to work fine; anything that either narrows OR binds
// crashes once a None-narrowing branch is ALSO present.
static bool pattern_is_risky_with_none(PyDAST *pattern) {
  if (!pattern) return false;
  if (pattern->kind == PY_name) {
    if (!strcmp(pattern->str_val, "_") || !strcmp(pattern->str_val, "None")) return false;
    return true;  // True/False (narrows), or a plain capture (binds)
  }
  if (pattern->kind == PY_number || pattern->kind == PY_string) return true;
  if (pattern->kind == PY_list || pattern->kind == PY_tuple || pattern->kind == PY_dict) return true;
  if (pattern->kind == PY_power && pattern->children.n == 2 && pattern->children[1]->kind == PY_call) return true;
  if (pattern->kind == PY_binop && pattern->op == PY_OP_BITOR)
    return pattern_is_risky_with_none(pattern->children[0]) || pattern_is_risky_with_none(pattern->children[1]);
  return false;
}

// Helper: build if-else chain for PY_match_stmt
static void build_match_pyda(PyDAST *n, PycAST *ast, PycCompiler &ctx) {
  // children[0] = MATCH_KW, children[1] = subject (test), children[2..N-1] = case_blocks
  PyDAST *subject = n->children[1];
  build_if1_pyda(subject, ctx);
  PycAST *subject_ast = getAST(subject, ctx);
  if1_gen(if1, &ast->code, subject_ast->code);

  // issues/023: combining a `None` pattern with almost anything else
  // in the SAME match statement is confirmed to crash at RUNTIME
  // ("matching function not found") -- codegen fails to generate a
  // clone for one of the union's actually-reachable types once a
  // None-narrowing branch is followed by EITHER a different
  // isinstance-narrowing branch OR a capture-pattern binding for the
  // same subject. `case None:` combined with ONLY wildcard (`_`)
  // and/or other `None` arms is the one combination confirmed safe;
  // everything else -- captures, literals, True/False,
  // sequence/mapping/class patterns -- crashes. Tried three different
  // lowerings for None's own check (a method dispatch, the
  // ordinary-call isinstance() form, the raw prim_isinstance
  // primitive) -- all three fail identically, so this isn't fixable
  // from this lowering code; it looks like a deeper FA/codegen
  // clone-generation gap. Fail loudly at compile time instead of
  // silently emitting code that can crash -- matches this function's
  // existing precedent (or-pattern capture alternatives, positional
  // class patterns) of refusing unsupported combinations rather than
  // guessing.
  bool has_none = false, has_risky = false;
  for (int i = 2; i < n->children.n; i++) {
    PyDAST *case_block = n->children[i];
    // Mirrors the case_cond indexing dance in the main loop below
    // exactly (children.n varies with whether a guard is present).
    PyDAST *case_cond = case_block->children.n >= 3 ? case_block->children[1] : case_block->children[0];
    if (pattern_contains_none(case_cond)) has_none = true;
    if (pattern_is_risky_with_none(case_cond)) has_risky = true;
  }
  if (has_none && has_risky)
    fail("error line %d: this match statement combines a 'case None:' pattern with another pattern "
         "that either binds a name or needs type narrowing -- this combination is not supported (a "
         "known pyc code-generation limitation, not a source error): compiled code for it can crash "
         "at runtime. 'case None:' may only be combined with a wildcard ('case _:') and/or other "
         "'case None:' arms in the same match statement. Split it into its own match statement, or "
         "use a guard ('case x if x is None:') instead.",
         ctx.lineno);

  Code *chain = 0; // Default is do nothing
  for (int i = n->children.n - 1; i >= 2; i--) {
    PyDAST *case_block = n->children[i];
    PyDAST *case_cond = case_block->children[1];
    PyDAST *case_suite = case_block->children[2];
    PycAST *case_ast = getAST(case_block, ctx);

    Code *case_chain = 0;
    Code *case_then = 0;
    if (case_block->children.n == 3) {
      case_cond = case_block->children[1];
      case_suite = case_block->children[2];
    } else if (case_block->children.n >= 4) {
      case_cond = case_block->children[1];
      case_suite = case_block->children[3]; // Skip ':'
    } else {
      case_cond = case_block->children[0];
      case_suite = case_block->children[1];
    }
    build_if1_suite_pyda(case_suite, &case_then, ctx);
    PyDAST *guard = find_case_guard(case_block);

    // `case PATTERN if cond:` only matches when both the pattern AND
    // the guard hold. The guard is threaded INTO build_pattern_match
    // (rather than evaluated afterward here) so it runs exactly at
    // the point the pattern's own match -- and any bindings it
    // makes -- are established: trivial for capture patterns (whose
    // binding is unconditional, so "afterward, in this same flat
    // code stream" and "right where the binding happened" coincide),
    // but load-bearing for sequence patterns, whose element bindings
    // only exist along a nested if1_if's "then" branch (see
    // guarded_bool) -- `case [a, b] if a > b:` needs `a > b`
    // evaluated inside that branch, not after it.
    std::function<Sym *(Code **)> guard_fn;
    const std::function<Sym *(Code **)> *guard_ptr = nullptr;
    if (guard) {
      guard_fn = [&](Code **gcode) -> Sym * { return eval_case_guard(guard, gcode, case_ast, ctx); };
      guard_ptr = &guard_fn;
    }
    Sym *matched = build_pattern_match(case_cond, subject_ast->rval, &case_chain, case_ast, ctx, guard_ptr);
    if (matched == sym_true) {
      // Irrefutable pattern (wildcard or capture) with no guard --
      // run unconditionally, no check needed at all.
      if1_gen(if1, &case_chain, case_then);
    } else {
      if1_if(if1, &case_chain, 0, matched, case_then, 0, chain, 0, 0, case_ast);
    }
    chain = case_chain;
  }

  if1_gen(if1, &ast->code, chain);
}

// issues/001: at the point a capturing lambda/nested-def expression is
// evaluated (e.g. `make_adder`'s `return lambda x: x + n`), construct a
// fresh instance of the closure-carrier class synthesized for it in
// build_syms_pyda (ast->closure_cls) and populate one field per captured
// name with that name's *current* value -- a snapshot at creation time,
// correct for ordinary (non-`nonlocal`) closures. Mirrors gen_class_pyda's
// __init__ construction (plain sym_new + per-field sym_setter; no
// prototype/clone indirection needed since every field is set immediately
// here, unlike a real class's prototype which persists between separate
// __new__ calls -- see issue 017 for why that indirection matters there
// and not here). Returns the new instance Sym, which becomes the lambda/
// nested-def expression's rval instead of the raw Fun Sym `fn`.
static Sym *build_closure_instance_pyda(Sym *cls, PycAST *ast, Code **code, PycCompiler &ctx) {
  Sym *inst = new_sym(ast);
  if1_send(if1, code, 3, 1, sym_primitive, sym_new, cls, inst)->ast = ast;
  for (Sym *field : cls->has.values()) {
    Sym *val = field;
    // Transitive capture (issues/007 parameterized decorators): the
    // captured value may itself be a captured field of the CREATOR's
    // own carrier (e.g. `wrapper` capturing `n` two scopes up, with
    // the intermediate `dec` capturing it too). Read it through the
    // creator's self.field instead of the raw outer Sym -- a raw
    // reference here would be mis-promoted by the creator's
    // if1_fixup_nesting (it looks like a LOCALLY_NESTED local of the
    // creator) and corrupt the outer function's own reads.
    Sym *cin = ctx.scope_stack.last()->in;
    if (cin && cin != cls && cin->has.in(field) && ctx.fun() && ctx.fun()->self) {
      val = new_sym(ast);
      if1_send(if1, code, 4, 1, sym_operator, ctx.fun()->self, sym_period, if1_make_symbol(if1, field->name), val)
          ->ast = ast;
    }
    if1_send(if1, code, 5, 1, sym_operator, inst, sym_setter, if1_make_symbol(if1, field->name), val,
              new_sym(ast))
        ->ast = ast;
  }
  return inst;
}

// Assign an already-computed value Sym `val` to the (already-built)
// target `tgt`, emitting into `code`. Handles simple names,
// attribute targets (`self.x`), subscript targets (`a[i]`), and
// arbitrarily nested tuple/list targets, recursing on each element.
static void emit_assign_to_target(PyDAST *tgt, Sym *val, Code **code, PycAST *ast, PycCompiler &ctx) {
  PycAST *a = getAST(tgt, ctx);
  if (tgt->kind == PY_star_expr)
    // issues/024: PEP 3132 requires a starred target to appear
    // INSIDE a list/tuple target (`a, *b = ...`) -- a bare `*b = x`
    // is a SyntaxError in real Python (testlist's own grammar can't
    // tell the difference: a single-element target list passes
    // through unwrapped, same as a non-star single target does).
    // Message matches CPython's own wording for this exact case.
    fail("error line %d: starred assignment target must be in a list or tuple", ctx.lineno);
  if (tgt->kind == PY_tuple || tgt->kind == PY_testlist || tgt->kind == PY_exprlist) {
    int star_idx = -1;
    for (int j = 0; j < tgt->children.n; j++) {
      if (tgt->children[j]->kind != PY_star_expr) continue;
      if (star_idx >= 0)
        fail("error line %d: multiple starred expressions in assignment", ctx.lineno);
      star_idx = j;
    }
    if (star_idx < 0) {
      // Destructuring: pull out val[j] and assign it to element j. The
      // element may itself be a name, attribute, subscript, or a further
      // nested tuple -- recursion covers all of them. The previous code
      // required the tuple's own sym and moved val[j] straight into the
      // element's rval, which only worked for plain-name elements, so
      // `self.x, self.y = ...` and `a[i], a[j] = ...` failed with
      // "illegal destructuring" (issue 025 illegal-destructuring
      // frontier: attribute / subscript / nested targets).
      for (int j = 0; j < tgt->children.n; j++) {
        Sym *item = new_sym(ast);
        call_method(code, ast, val, sym___getitem__, item, 1, int64_constant(j));
        emit_assign_to_target(tgt->children[j], item, code, ast, ctx);
      }
      return;
    }
    // issues/024: extended iterable unpacking (PEP 3132) --
    // `a, *b = val`, `*a, b = val`, `a, *b, c = val`. Leading targets
    // (before the star) bind val[0..n_leading-1] positionally, same
    // as the non-star case; trailing targets (after the star) bind
    // the LAST n_trailing elements, positionally from the end (so
    // they're correct regardless of how many elements the star
    // itself ends up consuming); the star target binds a NEW list
    // (PEP 3132: always a list, even when val is a tuple/other
    // sequence -- unlike a plain slice, which would preserve val's
    // own type) holding everything in between. No length-mismatch
    // validation (real Python raises ValueError for "not enough
    // values to unpack") -- matches this function's own pre-existing
    // non-star behavior, which never bounds-checks `val[j]` either.
    int n_leading = star_idx;
    int n_trailing = tgt->children.n - star_idx - 1;
    for (int j = 0; j < n_leading; j++) {
      Sym *item = new_sym(ast);
      call_method(code, ast, val, sym___getitem__, item, 1, int64_constant(j));
      emit_assign_to_target(tgt->children[j], item, code, ast, ctx);
    }
    Sym *len_val = new_sym(ast);
    call_method(code, ast, val, make_symbol("__len__"), len_val, 0);
    Sym *limit = new_sym(ast);
    call_method(code, ast, len_val, make_symbol("__sub__"), limit, 1, int64_constant(n_trailing));
    Sym *star_list = new_sym(ast);
    if1_send(if1, code, 3, 1, sym_primitive, sym_make, sym_list, star_list)->ast = ast;
    // idx = n_leading; while idx < limit: star_list.append(val[idx]); idx = idx + 1
    Sym *idx = new_sym(ast);
    Code *before = 0, *cond = 0, *body = 0;
    if1_move(if1, &before, int64_constant(n_leading), idx, ast);
    Sym *cond_var = new_sym(ast);
    call_method(&cond, ast, idx, make_symbol("__lt__"), cond_var, 1, limit);
    Sym *item = new_sym(ast);
    call_method(&body, ast, val, sym___getitem__, item, 1, idx);
    Sym *append_result = new_sym(ast);
    call_method(&body, ast, star_list, sym_append, append_result, 1, item);
    Sym *next_idx = new_sym(ast);
    call_method(&body, ast, idx, make_symbol("__add__"), next_idx, 1, int64_constant(1));
    if1_move(if1, &body, next_idx, idx, ast);
    if1_loop(if1, code, if1_alloc_label(if1), if1_alloc_label(if1), cond_var, before, cond, 0, body, ast);
    emit_assign_to_target(tgt->children[star_idx]->children[0], star_list, code, ast, ctx);
    for (int k = 0; k < n_trailing; k++) {
      Sym *tidx = new_sym(ast);
      call_method(code, ast, limit, make_symbol("__add__"), tidx, 1, int64_constant(k));
      Sym *titem = new_sym(ast);
      call_method(code, ast, val, sym___getitem__, titem, 1, tidx);
      emit_assign_to_target(tgt->children[star_idx + 1 + k], titem, code, ast, ctx);
    }
  } else {
    if1_gen(if1, code, a->code);
    if (a->is_member)
      if1_send(if1, code, 5, 1, sym_operator, a->rval, sym_setter, a->sym, val, (ast->rval = new_sym(ast)))
          ->ast = ast;
    else if (a->is_object_index)
      if1_add_send_arg(if1, find_send(a->code), val);
    else
      if1_move(if1, code, val, a->sym);
  }
}

static void build_if1_assign_target(PyDAST *tgt, PycAST *v, PycAST *ast, PycCompiler &ctx) {
  build_if1_pyda(tgt, ctx);  // build the whole target tree once
  emit_assign_to_target(tgt, v->rval, &ast->code, ast, ctx);
}

static void build_if1_with_items(PyDAST *stmt_node, int item_idx, PycCompiler &ctx, PycAST *ast) {
  if (item_idx == stmt_node->children.n - 1) {
    PyDAST *body_node = stmt_node->children[item_idx];
    build_if1_pyda(body_node, ctx);
    PycAST *body = getAST(body_node, ctx);
    if1_gen(if1, &ast->code, body->code);
    return;
  }

  PyDAST *item_node = stmt_node->children[item_idx];
  PyDAST *cm_expr = item_node->children[0];
  
  build_if1_pyda(cm_expr, ctx);
  PycAST *cm = getAST(cm_expr, ctx);
  if1_gen(if1, &ast->code, cm->code);
  
  Sym *enter_val = new_sym(ast);
  call_method(&ast->code, ast, cm->rval, make_symbol("__enter__"), enter_val, 0);
  
  if (item_node->children.n == 2) {
    PyDAST *tgt = item_node->children[1];
    PycAST temp_v;
    temp_v.rval = enter_val;
    build_if1_assign_target(tgt, &temp_v, ast, ctx);
  }
  
  ctx.with_stack.add(WithCleanup{cm->rval, ctx.loop_depth});
  build_if1_with_items(stmt_node, item_idx + 1, ctx, ast);
  ctx.with_stack.n--;
  
  Sym *exit_val = new_sym(ast);
  call_method(&ast->code, ast, cm->rval, make_symbol("__exit__"), exit_val, 3, sym_nil, sym_nil, sym_nil);
}

// issue 027 feature: find the function stored under `name` on class
// `cls`'s prototype, own or inherited: a has[] field whose alias is a
// function Sym (the method-field/alias convention set in
// build_syms_pyda's PY_funcdef/PY_decorated method branches;
// gen_class_pyda's inherited-field copy loop relies on the same
// links). Used by PY_power's class-qualified attribute resolution.
static Sym *find_class_method_fn(Sym *cls, cchar *name) {
  if (!cls || !name) return nullptr;
  for (Sym *h : cls->has)
    if (h && h->name && !strcmp(h->name, name) && h->alias && h->alias->is_fun) return h->alias;
  for (Sym *inc : cls->includes)
    if (Sym *r = find_class_method_fn(inc, name)) return r;
  return nullptr;
}

static int build_if1_pyda(PyDAST *n, PycCompiler &ctx) {
  if (!n) return 0;
  PycAST *ast = getAST(n, ctx);
  ctx.node = n;
  ctx.lineno = n->line;

  switch (n->kind) {
    case PY_suite:
      for (auto c : n->children.values()) {
        build_if1_pyda(c, ctx);
        if1_gen(if1, &ast->code, getAST(c, ctx)->code);
      }
      return 0;

    case PY_funcdef: {
      // Re-enter scope from build_syms pass and generate closure
      reenter_scope_pyda(n, ctx);
      // issues/001: a nested def capturing enclosing-function locals gets
      // the same treatment as a capturing lambda (see PY_lambda below) --
      // fn->self must exist *before* the body is walked, since PY_name's
      // self.field rewrite for a captured-name reference needs it already
      // set at the point that reference is processed.
      Sym *closure_cls = ast->closure_cls;
      if (closure_cls) {
        // fn->self lives on the INTERNAL function Sym (ast->sym) --
        // gen_fun_pyda reads it from there; since issues/007's split
        // identity, ast->rval is the public-name variable, not the fn.
        ast->sym->self = new_sym(ast);
        ast->sym->self->must_implement_and_specialize(closure_cls);
      }
      // Process default exprs (pre-scope in build_syms)
      PyDAST *params = n->children[1];
      PyDAST *varargsl = (params->children.n > 0) ? params->children[0] : nullptr;
      if (varargsl)
        for (auto c : varargsl->children.values())
          if (c->kind == PY_arg_default) build_if1_pyda(c->children[1], ctx);
      // Process body (recurse to set up child AST codes)
      if (n->children.n >= 3) {
        PyDAST *body_node = n->children[2];
        if (body_node->kind == PY_suite) {
          for (auto c : body_node->children.values()) build_if1_pyda(c, ctx);
        } else {
          build_if1_pyda(body_node, ctx);
        }
      }
      gen_fun_pyda(n, ast, ctx);
      exit_scope(ctx);
      // issues/007 split identity: bind the public-name variable
      // (ast->rval) to the function value. For a capturing nested def
      // (issues/001) the value is the closure-carrier instance; for a
      // plain def it is the internal function Sym itself. Methods
      // (recognized by the alias link set in build_syms_pyda) are
      // installed into the class via the setter emitted there and get
      // no variable binding.
      bool fd_is_method = ast->rval && ast->rval->alias == ast->sym;
      if (closure_cls) {
        Sym *inst = build_closure_instance_pyda(closure_cls, ast, &ast->code, ctx);
        if1_move(if1, &ast->code, inst, ast->rval, ast);
      } else if (!fd_is_method && ast->rval != ast->sym && ast->sym->is_generator) {
        // issues/014: `ast->sym` is the coroutine body (yields become
        // co_yield in cg.cc; its C return type is forced to a raw
        // int64 handle there too, regardless of what FA infers here).
        // Calling it directly would hand the caller a raw handle, not
        // something with __iter__/__pyc_more__/__next__ -- so the
        // PUBLIC name is bound to a small synthesized wrapper Fun
        // instead (same "public name != internal Fun" split already
        // used for ordinary defs, one level further): call the
        // coroutine body, wrap its handle result in a
        // __pyc_generator__ instance, return that. This keeps
        // PY_for_stmt's generic __iter__/__pyc_more__/__next__
        // dispatch (python_ifa_build_if1.cc PY_for_stmt) completely
        // unmodified -- `for v in gen():` just sees an ordinary
        // object of an ordinary class.
        //
        // Positional-only argument forwarding: ast->sym->has (set by
        // if1_closure -- see gen_fun_pyda) is exactly the array
        // gen_fun_pyda built it from -- has[0] is the coroutine
        // body's own "value convention" placeholder (as.add(fn), see
        // gen_fun_pyda's plain-def branch), has[1..] the real
        // parameters, in order, already correctly built by the
        // ordinary build_syms/build_if1 pipeline (get_syms_args_pyda)
        // including whatever type constraints the user's annotations
        // gave them. The wrapper doesn't need its own copies of those
        // constraints -- it's a pure passthrough, so type checking
        // still happens where it already did, at this call site
        // against ast->sym's real formal pattern. Fresh Syms (not
        // ast->sym's own) because a formal Sym is tied to the Fun
        // it was built for; sharing identity across two Funs wasn't
        // risked here. *args/**kwargs/defaults/keyword-only are not
        // handled yet -- has[i] with i>0 is assumed to be a plain
        // positional formal for every i (true for the shape
        // get_syms_args_pyda builds today; see its PY_star_arg /
        // PY_dstar_arg / PY_arg_default handling for what a fuller
        // version of this would need to mirror).
        Sym *wrapper = new_fun(ast);
        wrapper->nesting_depth = ast->sym->nesting_depth;
        Vec<Sym *> wrapper_formals;
        for (int i = 1; i < ast->sym->has.n; i++) wrapper_formals.add(new_sym(ast));
        Code *wbody = 0;
        Code *call_send = if1_send1(if1, &wbody, ast);
        if1_add_send_arg(if1, call_send, ast->sym);
        for (Sym *wf : wrapper_formals.values()) if1_add_send_arg(if1, call_send, wf);
        Sym *handle_result = new_sym(ast);
        if1_add_send_result(if1, call_send, handle_result);
        int lvl = 0;
        PycSymbol *gen_cls_ps = find_PycSymbol(ctx, cannonicalize_string("__pyc_generator__"), &lvl);
        if (!gen_cls_ps || !gen_cls_ps->sym)
          fail("error line %d, internal: __pyc_generator__ not found (issues/014)", ctx.lineno);
        Code *ctor_send = if1_send1(if1, &wbody, ast);
        if1_add_send_arg(if1, ctor_send, gen_cls_ps->sym);
        if1_add_send_arg(if1, ctor_send, handle_result);
        Sym *gen_inst = new_sym(ast);
        if1_add_send_result(if1, ctor_send, gen_inst);
        if1_move(if1, &wbody, gen_inst, wrapper->ret, ast);
        if1_send(if1, &wbody, 4, 0, sym_primitive, sym_reply, wrapper->cont, wrapper->ret)->ast = ast;
        Vec<Sym *> was;
        was.add(wrapper);
        for (Sym *wf : wrapper_formals.values()) was.add(wf);
        if1_closure(if1, wrapper, wbody, was.n, was.v);
        if1_move(if1, &ast->code, wrapper, ast->rval, ast);
      } else if (!fd_is_method && ast->rval != ast->sym) {
        // rval == sym is the legacy direct binding (non-RECORD
        // class-body defs) -- no variable to bind.
        if1_move(if1, &ast->code, ast->sym, ast->rval, ast);
      }
      return 0;
    }

    case PY_decorated: {
      PyDAST *def = n->children.last();
      // Process decorators
      for (int i = 0; i < n->children.n - 1; i++) build_if1_pyda(n->children[i], ctx);
      if (def->kind == PY_funcdef) {
        PycAST *def_ast = getAST(def, ctx);
        reenter_scope_pyda(def, ctx);
        PyDAST *params = def->children[1];
        PyDAST *varargsl = (params->children.n > 0) ? params->children[0] : nullptr;
        if (varargsl)
          for (auto c : varargsl->children.values())
            if (c->kind == PY_arg_default) build_if1_pyda(c->children[1], ctx);
        if (def->children.n >= 3) {
          PyDAST *bn = def->children[2];
          if (bn->kind == PY_suite) { for (auto c : bn->children.values()) build_if1_pyda(c, ctx); }
          else build_if1_pyda(bn, ctx);
        }
        gen_fun_pyda(def, def_ast, ctx);
        exit_scope(ctx);
        // issues/007: actually APPLY the decorators. With split
        // identity (build_syms_pyda), the def's public name
        // (def_ast->rval) is an ordinary variable and the raw
        // function is the internal Sym (def_ast->sym) -- so
        // decoration is just `name = dN(...(d1(fn)))`, ordinary
        // sends and moves. Decorators apply bottom-up (innermost
        // first), each one's expression evaluated just before its
        // application. Only plain-name decorators (optionally with
        // an argument list, applied as `d(args)(fn)`) are handled;
        // dotted names keep the historical silent no-op.
        Sym *cur = def_ast->sym;
        for (int i = n->children.n - 2; i >= 0; i--) {
          PyDAST *child = n->children[i];
          Vec<PyDAST *> decs;
          if (child->kind == PY_suite) {
            for (auto c : child->children.values()) decs.add(c);
          } else
            decs.add(child);
          for (int di = decs.n - 1; di >= 0; di--) {
            PyDAST *dec = decs[di];
            if (dec->kind != PY_decorator || dec->children.n < 1) continue;
            // issue 027 feature: @staticmethod/@classmethod are
            // definition markers consumed during build_syms_pyda,
            // not runtime decorators -- never applied.
            if (!(dec->children.n >= 2 && dec->children[1]->kind == PY_arglist) &&
                (decorator_name_is(dec->children[0]->str_val, "staticmethod") ||
                 decorator_name_is(dec->children[0]->str_val, "classmethod")))
              continue;
            PyDAST *fname_node = dec->children[0];
            PycAST *fname_ast = getAST(fname_node, ctx);
            Sym *dval = fname_ast->rval;
            if (!dval && fname_node->str_val && !strchr(fname_node->str_val, '.')) {
              // PY_dotted_name is a leaf with no build_syms case, so
              // no rval was set; resolve a plain name through the
              // scope stack ourselves. The decorator is an ordinary
              // variable under issues/007's split identity -- read it
              // through a fresh temp (the issues/031 load pattern).
              int lvl = 0;
              PycSymbol *dps = find_PycSymbol(ctx, cannonicalize_string(fname_node->str_val), &lvl);
              if (dps && dps->sym) {
                Sym *t = new_sym(ast);
                if1_move(if1, &def_ast->code, dps->sym, t, ast);
                dval = t;
              }
            }
            if (!dval) continue;  // dotted/unresolved: historical no-op
            if1_gen(if1, &def_ast->code, fname_ast->code);
            if (dec->children.n >= 2 && dec->children[1]->kind == PY_arglist) {
              // @d(args): evaluate d(args) first, then apply.
              PyDAST *arglist = dec->children[1];
              Code *send = if1_send1(if1, &def_ast->code, ast);
              if1_add_send_arg(if1, send, dval);
              for (auto a : arglist->children.values()) {
                PycAST *aast = getAST(a, ctx);
                if1_gen(if1, &def_ast->code, aast->code);
                if1_add_send_arg(if1, send, aast->rval);
              }
              Sym *dv2 = new_sym(ast);
              if1_add_send_result(if1, send, dv2);
              dval = dv2;
            }
            Sym *res = new_sym(ast);
            if1_send(if1, &def_ast->code, 2, 1, dval, cur, res)->ast = ast;
            cur = res;
          }
        }
        // Marker methods (@staticmethod/@classmethod) use the method-
        // FIELD shape (rval->alias == sym, installed via the setter
        // emitted in build_syms_pyda) -- no variable binding, same rule
        // as PY_funcdef's fd_is_method.
        if (!(def_ast->rval && def_ast->rval->alias == def_ast->sym))
          if1_move(if1, &def_ast->code, cur, def_ast->rval, ast);
        ast->rval = def_ast->rval;
        ast->code = def_ast->code;
      } else if (def->kind == PY_classdef) {
        PycAST *def_ast = getAST(def, ctx);
        // Scan decorators.  @vector("s") is builtin-only (it sets
        // up the trailing-array layout used by `bytearray`).
        // @pyc_struct works for any class (issue 015) — it sets
        // is_value_type on the class Sym, which the IFA's
        // set_value_for_value_classes pass then lifts through
        // `implements` to subclasses.
        char *vector_size = nullptr;
        for (int i = 0; i < n->children.n - 1; i++) {
          PyDAST *child = n->children[i];
          // Decorators may be wrapped in a PY_suite or be direct PY_decorator nodes
          Vec<PyDAST *> decs;
          if (child->kind == PY_suite) {
            for (auto c : child->children.values()) decs.add(c);
          } else {
            decs.add(child);
          }
          for (auto dec : decs.values()) {
            if (dec->kind != PY_decorator || dec->children.n < 1) continue;
            PyDAST *fname_node = dec->children[0];
            cchar *fname = (fname_node->kind == PY_dotted_name || fname_node->kind == PY_name)
                            ? fname_node->str_val : nullptr;
            if (!fname) continue;
            if (ctx.is_builtin() && strcmp(fname, "vector") == 0 &&
                dec->children.n >= 2) {
              // @vector("s") — extract string arg from arglist
              PyDAST *arglist = dec->children[1];
              if (arglist->kind == PY_arglist && arglist->children.n > 0) {
                PyDAST *arg = arglist->children[0];
                if (arg->kind == PY_string && arg->str_val) {
                  cchar *s = arg->str_val;
                  int len = strlen(s);
                  char *inner = (char *)MALLOC(len + 1);
                  if (len >= 2 && (s[0] == '"' || s[0] == '\'')) {
                    strncpy(inner, s + 1, len - 2);
                    inner[len - 2] = 0;
                  } else {
                    strcpy(inner, s);
                  }
                  vector_size = inner;
                  def_ast->sym->is_vector = 1;
                  if (!def_ast->sym->element) def_ast->sym->element = new_sym(def_ast);
                }
              }
            } else if (strcmp(fname, "pyc_struct") == 0) {
              // @pyc_struct — POD/value-type record opt-in
              // (issue 015).  No args required; an empty
              // arglist `@pyc_struct()` is also accepted.
              // set_value_for_value_classes (ifa/if1/ast.cc)
              // then lifts the bit transitively through
              // `implements` so subclasses inherit.
              def_ast->sym->is_value_type = 1;
            }
          }
        }
        reenter_scope_pyda(def, ctx);
        {
          PyDAST *bn = def->children.last();
          if (bn->kind == PY_suite) { for (auto c : bn->children.values()) build_if1_pyda(c, ctx); }
          else build_if1_pyda(bn, ctx);
        }
        gen_class_pyda(def, def_ast, ctx, vector_size);
        exit_scope(ctx);
        ast->rval = def_ast->rval;
        ast->code = def_ast->code;
      }
      return 0;
    }

    case PY_classdef: {
      reenter_scope_pyda(n, ctx);
      // Process base class exprs
      for (int i = 1; i < n->children.n - 1; i++) build_if1_pyda(n->children[i], ctx);
      // Process body (last child = PY_suite or single stmt)
      {
        PyDAST *bn = n->children.last();
        if (bn->kind == PY_suite) { for (auto c : bn->children.values()) build_if1_pyda(c, ctx); }
        else build_if1_pyda(bn, ctx);
      }
      gen_class_pyda(n, ast, ctx);
      exit_scope(ctx);
      return 0;
    }

    case PY_lambda: {
      // Re-enter scope from build_syms pass
      reenter_scope_pyda(n, ctx);
      Sym *closure_cls = ast->closure_cls;
      if (closure_cls) {
        // issues/001: fn->self must exist *before* the body is walked
        // below -- PY_name's self.field rewrite for a captured-name
        // reference needs ctx.fun()->self already set at the point that
        // reference is processed, not merely by the time gen_lambda_pyda
        // (which reuses the body's already-computed code/rval) runs
        // afterward.
        ast->rval->self = new_sym(ast);
        ast->rval->self->must_implement_and_specialize(closure_cls);
      }
      PyDAST *varargsl = (n->children.n > 0 && n->children[0]->kind == PY_varargslist) ? n->children[0] : nullptr;
      if (varargsl)
        for (auto c : varargsl->children.values())
          if (c->kind == PY_arg_default) build_if1_pyda(c->children[1], ctx);
      build_if1_pyda(n->children.last(), ctx);
      gen_lambda_pyda(n, ast, ctx);
      exit_scope(ctx);
      if (closure_cls) ast->rval = build_closure_instance_pyda(closure_cls, ast, &ast->code, ctx);
      return 0;
    }

    case PY_return_stmt: {
      if (n->children.n > 0) {
        build_if1_pyda(n->children[0], ctx);
        PycAST *val = getAST(n->children[0], ctx);
        ctx.fun()->fun_returns_value = 1;
        if1_gen(if1, &ast->code, val->code);
        if1_move(if1, &ast->code, val->rval, ctx.fun()->ret, ast);
      } else
        // issues/014: keep a bare `return` inside a generator body
        // consistent with gen_fun_pyda's int64-typed default reply
        // (see its comment) -- both exit paths must agree on the
        // Fun's inferred return type.
        if1_move(if1, &ast->code, ctx.fun()->is_generator ? int64_constant(0) : sym_nil, ctx.fun()->ret, ast);
      
      // Emit cleanups for all active with statements in this function
      for (int i = ctx.with_stack.n - 1; i >= 0; i--) {
        Sym *exit_val = new_sym(ast);
        call_method(&ast->code, ast, ctx.with_stack[i].cm_rval, make_symbol("__exit__"), exit_val, 3, sym_nil, sym_nil, sym_nil);
      }
      
      if1_goto(if1, &ast->code, ast->label[0])->ast = ast;
      return 0;
    }

    case PY_assign: {
      // children: [target0, ..., value] (last is value)
      PyDAST *val_node = n->children.last();
      build_if1_pyda(val_node, ctx);
      PycAST *v = getAST(val_node, ctx);
      if1_gen(if1, &ast->code, v->code);
      for (int i = 0; i < n->children.n - 1; i++) {
        PyDAST *tgt = n->children[i];
        build_if1_assign_target(tgt, v, ast, ctx);
      }
      return 0;
    }

    case PY_namedexpr_test: {
      PyDAST *tgt = n->children[0];
      PyDAST *val_node = n->children[1];
      build_if1_pyda(val_node, ctx);
      PycAST *v = getAST(val_node, ctx);
      if1_gen(if1, &ast->code, v->code);
      build_if1_pyda(tgt, ctx);
      PycAST *a = getAST(tgt, ctx);
      if1_gen(if1, &ast->code, a->code);
      if (a->is_member) {
        if1_send(if1, &ast->code, 5, 1, sym_operator, a->rval, sym_setter, a->sym, v->rval,
                 new_sym(ast));
      } else if (a->is_object_index) {
        if1_add_send_arg(if1, find_send(a->code), v->rval);
      } else {
        if1_move(if1, &ast->code, v->rval, a->sym);
      }
      ast->rval = v->rval; // Return the assigned value
      return 0;
    }

    case PY_annassign: {
      if (n->children.n == 3) {
        PyDAST *tgt = n->children[0];
        PyDAST *val_node = n->children[2];
        build_if1_pyda(val_node, ctx);
        PycAST *v = getAST(val_node, ctx);
        if1_gen(if1, &ast->code, v->code);
        build_if1_pyda(tgt, ctx);
        PycAST *a = getAST(tgt, ctx);
        if1_gen(if1, &ast->code, a->code);
        if (a->is_member) {
          if1_send(if1, &ast->code, 5, 1, sym_operator, a->rval, sym_setter, a->sym, v->rval,
                   new_sym(ast));
        } else if (a->is_object_index) {
          if1_add_send_arg(if1, find_send(a->code), v->rval);
        } else {
          if1_move(if1, &ast->code, v->rval, a->sym);
        }
      }
      ast->rval = 0;
      return 0;
    }

    case PY_augassign: {
      // children: [target, PY_augassign_op_node, value] for statement; 0 children for operator node
      if (n->children.n < 3) return 0;  // Skip operator-only nodes
      PyDAST *tgt_node = n->children[0];
      int op = n->children[1]->op;
      PyDAST *val_node = n->children[2];
      build_if1_pyda(val_node, ctx);
      build_if1_pyda(tgt_node, ctx);
      PycAST *v = getAST(val_node, ctx);
      PycAST *t = getAST(tgt_node, ctx);
      if1_gen(if1, &ast->code, v->code);
      if1_gen(if1, &ast->code, t->code);
      if (t->is_member) {
        Sym *tmp = new_sym(ast), *tmp2 = new_sym(ast);
        if1_send(if1, &ast->code, 4, 1, sym_operator, t->rval, sym_period, t->sym, tmp2)->ast = ast;
        if1_send(if1, &ast->code, 3, 1, map_pyop_to_ioperator(op), tmp2, v->rval, tmp)->ast = ast;
        if1_send(if1, &ast->code, 5, 1, sym_operator, t->rval, sym_setter, t->sym, tmp, (ast->rval = new_sym(ast)))
            ->ast = ast;
      } else if (t->is_object_index) {
        if1_add_send_arg(if1, find_send(ast->code), v->rval);
        if1_send(if1, &ast->code, 3, 1, map_pyop_to_ioperator(op), t->rval, v->rval, (ast->rval = new_sym(ast)))
            ->ast = ast;
      } else {
        if1_send(if1, &ast->code, 3, 1, map_pyop_to_ioperator(op), t->rval, v->rval, (ast->rval = new_sym(ast)))
            ->ast = ast;
        if1_move(if1, &ast->code, ast->rval, t->sym, ast);
      }
      return 0;
    }

    case PY_for_stmt: {
      // children: [target, iter, PY_suite, PY_else_clause?]
      build_if1_pyda(n->children[1], ctx);  // iter
      build_if1_pyda(n->children[0], ctx);  // target
      PycAST *i_ast = getAST(n->children[1], ctx);
      Sym *iter = new_sym(ast), *tmp = new_sym(ast), *tmp2 = new_sym(ast);
      if1_gen(if1, &ast->code, i_ast->code);
      if1_send(if1, &ast->code, 2, 1, sym___iter__, i_ast->rval, iter)->ast = ast;
      Code *cond = 0, *body = 0, *orelse = 0, *next = 0;
      call_method(&cond, ast, iter, sym___pyc_more__, tmp, 0);
      call_method(&body, ast, iter, sym___next__, tmp2, 0);
      // Assign the __next__ result through the general target path:
      // handles `for i, c in zip(...)` tuple unpacking (and attribute
      // / subscript targets) instead of a raw move into t->sym, which
      // is null for a tuple target (issue 025 "has no type" bucket --
      // collatz's `for (i, c) in zip(...)`). The old unconditional
      // `if1_gen(t->code)` is gone with it: for a tuple target that
      // code was a spurious make-tuple over the not-yet-bound names.
      emit_assign_to_target(n->children[0], tmp2, &body, ast, ctx);
      ctx.loop_depth++;
      build_if1_suite_pyda(n->children[2], &body, ctx);
      ctx.loop_depth--;
      if (n->children.n > 3 && n->children[3]->kind == PY_else_clause)
        build_if1_suite_pyda(n->children[3]->children[0], &orelse, ctx);
      if1_loop(if1, &ast->code, ast->label[0], ast->label[1], tmp, 0, cond, next, body, ast);
      if1_gen(if1, &ast->code, orelse);
      return 0;
    }

    case PY_while_stmt: {
      // children: [cond, PY_suite, PY_else_clause?]
      build_if1_pyda(n->children[0], ctx);
      PycAST *t = getAST(n->children[0], ctx);
      Code *body = 0, *orelse = 0;
      ctx.loop_depth++;
      build_if1_suite_pyda(n->children[1], &body, ctx);
      ctx.loop_depth--;
      if (n->children.n > 2 && n->children[2]->kind == PY_else_clause)
        build_if1_suite_pyda(n->children[2]->children[0], &orelse, ctx);
      if1_loop(if1, &ast->code, ast->label[0], ast->label[1], t->rval, 0, t->code, 0, body, ast);
      if1_gen(if1, &ast->code, orelse);
      return 0;
    }

    case PY_if_stmt:
      build_if_pyda(n, ast, ctx);
      return 0;

    case PY_match_stmt:
      build_match_pyda(n, ast, ctx);
      return 0;

    case PY_elif_clause: {
      // Handled by build_if_pyda
      build_if_pyda(n, ast, ctx);
      return 0;
    }

    case PY_import_name: {
      // Build a lambda to call build_import_syms with pyda info
      auto do_import = [&](cchar *sym_name, cchar *as_name, cchar *from_mod) {
        build_import_if1(const_cast<char *>(sym_name), const_cast<char *>(as_name),
                         const_cast<char *>(from_mod), ctx);
      };
      Vec<PyDAST *> names;
      for (auto c : n->children.values()) {
        if (c->kind == PY_dotted_as_name)
          names.add(c);
        else if (c->kind == PY_testlist)
          for (auto cc : c->children.values())
            if (cc->kind == PY_dotted_as_name) names.add(cc);
      }
      for (auto d : names.values()) {
        cchar *mod_name = d->children[0]->str_val;
        cchar *as_name = (d->children.n > 1) ? d->children[1]->str_val : nullptr;
        do_import(mod_name, as_name, nullptr);
      }
      return 0;
    }

    case PY_import_from: {
      if (n->children.n < 1) return 0;
      cchar *from_mod = n->children[0]->str_val;
      bool any = false;
      for (int i = 1; i < n->children.n; i++) {
        PyDAST *child = n->children[i];
        if (child->kind == PY_testlist) {
          for (auto ia : child->children.values())
            if (ia->kind == PY_import_as_name) {
              cchar *sym_name = ia->children[0]->str_val;
              cchar *as_name = (ia->children.n > 1) ? ia->children[1]->str_val : nullptr;
              build_import_if1(const_cast<char *>(sym_name), const_cast<char *>(as_name),
                               const_cast<char *>(from_mod), ctx);
              any = true;
            }
        } else if (child->kind == PY_import_as_name) {
          cchar *sym_name = child->children[0]->str_val;
          cchar *as_name = (child->children.n > 1) ? child->children[1]->str_val : nullptr;
          build_import_if1(const_cast<char *>(sym_name), const_cast<char *>(as_name),
                           const_cast<char *>(from_mod), ctx);
          any = true;
        }
      }
      // `from X import *`: no import_as_name children; still need the
      // module's if1 built (names were bound during build_syms).
      if (!any) build_import_if1(nullptr, nullptr, const_cast<char *>(from_mod), ctx);
      return 0;
    }

    case PY_global_stmt:
      return 0;

    case PY_pass_stmt:
    case PY_del_stmt:
      return 0;

    case PY_break_stmt:
    case PY_continue_stmt: {
      // Emit cleanups for active with statements within this loop
      for (int i = ctx.with_stack.n - 1; i >= 0; i--) {
        if (ctx.with_stack[i].loop_depth < ctx.loop_depth) break; // Exited the loop's bounds
        Sym *exit_val = new_sym(ast);
        call_method(&ast->code, ast, ctx.with_stack[i].cm_rval, make_symbol("__exit__"), exit_val, 3, sym_nil, sym_nil, sym_nil);
      }
      if1_goto(if1, &ast->code, ast->label[0])->ast = ast;
      return 0;
    }

    case PY_expr_stmt: {
      // Single expression statement (function call, etc.)
      if (n->children.n == 1) {
        build_if1_pyda(n->children[0], ctx);
        PycAST *e = getAST(n->children[0], ctx);
        ast->rval = e->rval;
        ast->code = e->code;
      }
      return 0;
    }

    case PY_bool_or:
    case PY_bool_and: {
      bool is_and = (n->kind == PY_bool_and);
      int nc = n->children.n;
      if (nc == 1) {
        build_if1_pyda(n->children[0], ctx);
        PycAST *v = getAST(n->children[0], ctx);
        ast->code = v->code;
        ast->rval = v->rval;
        return 0;
      }
      // BOOLEAN CONTEXT: when the and/or feeds an if/while/elif
      // condition (or `not`, or another and/or that itself feeds
      // one), Python's operand-VALUE result is unobservable -- only
      // its truthiness is consumed -- so the result var can be the
      // per-operand __pyc_to_bool__ BOOL instead of the operand
      // value. The value form unions every operand's type with bool
      // in one var ({nil, list, bool} for the ubiquitous
      // `if node.args and <test>:` optional-field idiom), and that
      // union is both un-layout-able (1-byte bool vs 8-byte
      // pointers: clone.cc "mismatched field sizes" via the
      // partial-application closure) and needlessly polymorphic.
      // genetic2's get_random_node/crossover guards were the
      // motivating case (pyc issues/025).
      auto in_boolean_context = [](PyDAST *nn) {
        for (PyDAST *p = nn->parent; p; nn = p, p = p->parent) {
          if ((p->kind == PY_if_stmt || p->kind == PY_while_stmt || p->kind == PY_elif_clause) &&
              p->children.n && p->children[0] == nn)
            return true;
          if (p->kind == PY_bool_not) return true;
          if (p->kind == PY_bool_and || p->kind == PY_bool_or) continue;  // keep walking up
          return false;
        }
        return false;
      };
      bool bool_ctx = in_boolean_context(n);
      ast->label[0] = if1_alloc_label(if1);
      ast->rval = new_sym(ast);
      for (int i = 0; i < nc - 1; i++) {
        build_if1_pyda(n->children[i], ctx);
        PycAST *v = getAST(n->children[i], ctx);
        if1_gen(if1, &ast->code, v->code);
        Sym *t = new_sym(ast);
        call_method(&ast->code, ast, v->rval, sym___pyc_to_bool__, t, 0);
        if (bool_ctx)
          if1_move(if1, &ast->code, t, ast->rval);
        else
          if1_move(if1, &ast->code, v->rval, ast->rval);
        Code *ifcode = if1_if_goto(if1, &ast->code, t, ast);
        if (is_and) {
          if1_if_label_false(if1, ifcode, ast->label[0]);
          if1_if_label_true(if1, ifcode, if1_label(if1, &ast->code, ast));
        } else {
          if1_if_label_true(if1, ifcode, ast->label[0]);
          if1_if_label_false(if1, ifcode, if1_label(if1, &ast->code, ast));
        }
      }
      build_if1_pyda(n->children[nc - 1], ctx);
      PycAST *v = getAST(n->children[nc - 1], ctx);
      if1_gen(if1, &ast->code, v->code);
      if (bool_ctx) {
        Sym *t = new_sym(ast);
        call_method(&ast->code, ast, v->rval, sym___pyc_to_bool__, t, 0);
        if1_move(if1, &ast->code, t, ast->rval, ast);
      } else {
        if1_move(if1, &ast->code, v->rval, ast->rval, ast);
      }
      if1_label(if1, &ast->code, ast, ast->label[0]);
      return 0;
    }

    case PY_bool_not: {
      build_if1_pyda(n->children[0], ctx);
      PycAST *v = getAST(n->children[0], ctx);
      if1_gen(if1, &ast->code, v->code);
      ast->rval = new_sym(ast);
      // Period-dispatch (call_method), not a bare selector send: the
      // operand can be any object (`not self.field`, None|T optional
      // unions), and only the `.` dispatch path resolves inherited
      // methods like __pyc_any_type__.__not__. The bare selector form
      // only ever matched bool.__not__, so `not <object>` failed at
      // runtime with "matching function not found" (issue 025).
      call_method(&ast->code, ast, v->rval, make_symbol("__not__"), ast->rval, 0);
      return 0;
    }

    case PY_compare: {
      // children: [left, cmp_op, right, cmp_op, right, ...]
      int nc = n->children.n;
      build_if1_pyda(n->children[0], ctx);
      PycAST *lv = getAST(n->children[0], ctx);
      if1_gen(if1, &ast->code, lv->code);
      int n_pairs = (nc - 1) / 2;  // each pair is (cmp_op, right)
      if (n_pairs == 1) {
        build_if1_pyda(n->children[2], ctx);
        PycAST *rv = getAST(n->children[2], ctx);
        if1_gen(if1, &ast->code, rv->code);
        ast->rval = new_sym(ast);
        int op = n->children[1]->op;
        // Issue 024 / 025: lower `x is None` and `None is x`
        // directly to prim_isinstance(operand, sym_nil_type)
        // instead of the __is__ method dispatch.  The method
        // dispatch fails on union receivers (pyc IFA doesn't
        // split functions per receiver type for method-only
        // bodies), but prim_isinstance is a true primitive
        // that IFA's splitter already handles correctly and
        // narrows the operand at the conditional branch (see
        // issue 025 Code_IF narrowing infrastructure).
        bool lv_is_none = (lv->rval == sym_nil);
        bool rv_is_none = (rv->rval == sym_nil);
        if ((op == PY_CMP_IS || op == PY_CMP_IS_NOT) &&
            (lv_is_none || rv_is_none) &&
            !(lv_is_none && rv_is_none)) {
          Sym *operand = lv_is_none ? rv->rval : lv->rval;
          Sym *iso = make_symbol("isinstance");
          if (op == PY_CMP_IS) {
            // x is None  →  isinstance(x, __pyc_None_type__)
            if1_send(if1, &ast->code, 4, 1, sym_primitive,
                     iso, operand, sym_nil_type, ast->rval)->ast = ast;
          } else {
            // x is not None  →  not isinstance(x, __pyc_None_type__)
            Sym *tmp = new_sym(ast);
            if1_send(if1, &ast->code, 4, 1, sym_primitive,
                     iso, operand, sym_nil_type, tmp)->ast = ast;
            if1_send(if1, &ast->code, 2, 1,
                     make_symbol("__not__"), tmp, ast->rval)->ast = ast;
          }
        } else if ((op == PY_CMP_IS || op == PY_CMP_IS_NOT) &&
                   lv_is_none && rv_is_none) {
          // None is None  →  True; None is not None  →  False
          if1_move(if1, &ast->code,
                   op == PY_CMP_IS ? sym_true : sym_false,
                   ast->rval, ast);
        } else if (op == PY_CMP_IS || op == PY_CMP_IS_NOT) {
          // Issue 028 step 4: real identity comparison for
          // non-None operands.  Previously routed to the
          // `__is__` method (inherited from
          // `__pyc_any_type__`) which always returned False
          // — broke `z is z.next`-style single-node-ring
          // checks.  prim_is is a pointer-equality primitive
          // at codegen, matching CPython's identity
          // semantics for non-None operands.  The `is not`
          // form negates via __not__ (mirrors the
          // `is not None` lowering above).
          Sym *iso = make_symbol("is");
          if (op == PY_CMP_IS) {
            if1_send(if1, &ast->code, 4, 1, sym_primitive,
                     iso, lv->rval, rv->rval, ast->rval)->ast = ast;
          } else {
            Sym *tmp = new_sym(ast);
            if1_send(if1, &ast->code, 4, 1, sym_primitive,
                     iso, lv->rval, rv->rval, tmp)->ast = ast;
            if1_send(if1, &ast->code, 2, 1,
                     make_symbol("__not__"), tmp, ast->rval)->ast = ast;
          }
        } else if (op == PY_CMP_IN || op == PY_CMP_NOT_IN) {
          emit_in_pyda(&ast->code, ast, lv->rval, rv->rval, op, ast->rval);
        } else {
          if1_send(if1, &ast->code, 3, 1, map_pyop_to_cmp(op),
                   lv->rval, rv->rval, ast->rval)->ast = ast;
        }
      } else {
        ast->label[0] = if1_alloc_label(if1);
        ast->label[1] = if1_alloc_label(if1);
        ast->rval = new_sym(ast);
        Sym *ls = lv->rval, *s = 0;
        for (int i = 0; i < n_pairs; i++) {
          PyDAST *op_node = n->children[1 + i * 2];
          PyDAST *rhs_node = n->children[2 + i * 2];
          build_if1_pyda(rhs_node, ctx);
          PycAST *rv = getAST(rhs_node, ctx);
          if1_gen(if1, &ast->code, rv->code);
          s = new_sym(ast);
          if (op_node->op == PY_CMP_IN || op_node->op == PY_CMP_NOT_IN) {
            emit_in_pyda(&ast->code, ast, ls, rv->rval, op_node->op, s);
          } else {
            if1_send(if1, &ast->code, 3, 1, map_pyop_to_cmp(op_node->op), ls, rv->rval, s)->ast = ast;
          }
          ls = rv->rval;
          Code *ifcode = if1_if_goto(if1, &ast->code, s, ast);
          if1_if_label_false(if1, ifcode, ast->label[0]);
          if1_if_label_true(if1, ifcode, if1_label(if1, &ast->code, ast));
        }
        if1_move(if1, &ast->code, s, ast->rval);
        if1_goto(if1, &ast->code, ast->label[1]);
        if1_label(if1, &ast->code, ast, ast->label[0]);
        if1_move(if1, &ast->code, sym_false, ast->rval, ast);
        if1_label(if1, &ast->code, ast, ast->label[1]);
      }
      return 0;
    }

    case PY_binop: {
      // children: [left, right] with op in n->op
      if (n->op == PY_OP_ADD || n->op == PY_OP_MUL) {
        Vec<PyDAST *> flat;
        if (try_fold_tuple_arity(n, flat)) {
          Vec<PyDAST *> uniq;
          for (auto c : flat.values())
            if (!uniq.in(c)) uniq.add(c);
          for (auto c : uniq.values()) {
            build_if1_pyda(c, ctx);
            if1_gen(if1, &ast->code, getAST(c, ctx)->code);
          }
          Code *send = if1_send1(if1, &ast->code, ast);
          if1_add_send_arg(if1, send, sym_primitive);
          if1_add_send_arg(if1, send, sym_make);
          if1_add_send_arg(if1, send, sym_tuple);
          for (auto c : flat.values()) if1_add_send_arg(if1, send, getAST(c, ctx)->rval);
          ast->rval = new_sym(ast);
          if1_add_send_result(if1, send, ast->rval);
          return 0;
        }
      }
      build_if1_pyda(n->children[0], ctx);
      build_if1_pyda(n->children[1], ctx);
      PycAST *lv = getAST(n->children[0], ctx);
      PycAST *rv = getAST(n->children[1], ctx);
      if1_gen(if1, &ast->code, lv->code);
      // `fmt % args` with a CONSTANT format string: pre-convert each
      // %s argument through __str__ (full method dispatch) before the
      // __mod__ send. The __pyc_format_string__ primitive passes args
      // RAW into C varargs, so a %s spec receiving an int64/float64
      // strlen'd the scalar (segfault: genetic2's
      // `"Epoch: %s, best fitness: %s" % (epoch, fitness)`) and an
      // object could never print its __str__. %d/%f/etc. args stay
      // raw (matching specs already work). Non-constant formats keep
      // the old path. For a literal arg tuple, the conversions are
      // emitted BEFORE the tuple's own make send and the make's args
      // are swapped in place (building a second tuple would leave the
      // original het-tuple make dead but diagnosed).
      if (n->op == PY_OP_MOD && lv->rval && lv->rval->type == sym_string && lv->rval->constant) {
        Vec<char> convs;  // conversion char per % spec, in order
        for (cchar *p = lv->rval->constant; *p; p++) {
          if (*p != '%') continue;
          p++;
          if (*p == '%') continue;                            // literal %%
          while (*p && (strchr("-+ #0", *p) || (*p >= '0' && *p <= '9') || *p == '.')) p++;
          if (*p) convs.add(*p);
        }
        if (convs.n && n->children[1]->kind == PY_tuple && n->children[1]->children.n == convs.n) {
          // Generate the ELEMENTS' code directly (the literal tuple's
          // own make send is never generated, so no dead het-tuple
          // remains to diagnose), stringify the %s members, then make
          // a fresh arg tuple from the converted values.
          for (auto c : n->children[1]->children.values())
            if1_gen(if1, &ast->code, getAST(c, ctx)->code);
          Vec<Sym *> argv;
          for (int i = 0; i < convs.n; i++) {
            Sym *av = getAST(n->children[1]->children[i], ctx)->rval;
            if (convs[i] == 's') {
              Sym *sv = new_sym(ast);
              call_method(&ast->code, ast, av, sym___str__, sv, 0);
              argv.add(sv);
            } else {
              argv.add(av);
            }
          }
          Code *send = if1_send1(if1, &ast->code, ast);
          if1_add_send_arg(if1, send, sym_primitive);
          if1_add_send_arg(if1, send, sym_make);
          if1_add_send_arg(if1, send, sym_tuple);
          for (Sym *av : argv) if1_add_send_arg(if1, send, av);
          Sym *targs = new_sym(ast);
          if1_add_send_result(if1, send, targs);
          ast->rval = new_sym(ast);
          if1_send(if1, &ast->code, 3, 1, make_symbol("__mod__"), lv->rval, targs, ast->rval)->ast = ast;
          return 0;
        }
        if (convs.n == 1 && convs[0] == 's' && n->children[1]->kind != PY_tuple) {
          // Single non-tuple %s argument.
          if1_gen(if1, &ast->code, rv->code);
          Sym *sv = new_sym(ast);
          call_method(&ast->code, ast, rv->rval, sym___str__, sv, 0);
          ast->rval = new_sym(ast);
          if1_send(if1, &ast->code, 3, 1, make_symbol("__mod__"), lv->rval, sv, ast->rval)->ast = ast;
          return 0;
        }
      }
      if1_gen(if1, &ast->code, rv->code);
      ast->rval = new_sym(ast);
      if1_send(if1, &ast->code, 3, 1, map_pyop_to_operator(n->op), lv->rval, rv->rval, ast->rval)->ast = ast;
      return 0;
    }

    case PY_unaryop: {
      build_if1_pyda(n->children[0], ctx);
      PycAST *v = getAST(n->children[0], ctx);
      if1_gen(if1, &ast->code, v->code);
      ast->rval = new_sym(ast);
      if1_send(if1, &ast->code, 2, 1, map_pyop_to_unary(n->op), v->rval, ast->rval)->ast = ast;
      return 0;
    }

    case PY_power: {
      // children: [atom, trailer1, trailer2, ...]
      build_if1_pyda(n->children[0], ctx);
      PycAST *atom_ast = getAST(n->children[0], ctx);
      if1_gen(if1, &ast->code, atom_ast->code);
      Sym *cur_val = atom_ast->rval;
      bool pending_member = false;
      Sym *pending_sym = nullptr;
      // issue 027 feature: class-qualified member state, parallel to
      // pending_member/pending_sym. qual_cls is the class Sym `X` in
      // `X.attr` when X is a literal class reference and attr names a
      // method-valued prototype field; qual_fn is additionally set for
      // an @classmethod. Consumed by the PY_call trailer; every site
      // that consumes or flushes pending_member resets them.
      Sym *qual_cls = nullptr, *qual_fn = nullptr;
      for (int i = 1; i < n->children.n; i++) {
        PyDAST *trailer = n->children[i];
        if (trailer->kind == PY_attribute) {
          // `X.attr` where X is an imported module (issue 025 module
          // subsystem phase 2): a module is a compile-time namespace,
          // not a runtime object, so resolve attr directly to the
          // module member symbol instead of emitting a `.` dispatch.
          if (!pending_member && cur_val && cur_val->is_module) {
            PycModule *mm = ctx.module_syms.get(cur_val);
            if (mm) {
              cchar *attr = trailer->children[0]->str_val;
              PycScope *ms = mm->ctx->saved_scopes.get(mm->pymod);
              PycSymbol *member = ms ? ms->map.get(cannonicalize_string(attr)) : nullptr;
              if ((intptr_t)member <= (intptr_t)NONLOCAL_DEF)
                fail("error line %d, module '%s' has no attribute '%s'", ctx.lineno, mm->name, attr);
              cur_val = member->sym;
              continue;
            }
          }
          // Flush previous pending member
          if (pending_member) {
            Sym *t = new_sym(ast);
            Sym *obj = cur_val->self ? cur_val->self : cur_val;
            Code *send = if1_send(if1, &ast->code, 4, 1, sym_operator, obj, sym_period, pending_sym, t);
            send->ast = ast;
            send->partial = Partial_OK;
            cur_val = t;
            pending_member = false;
            qual_cls = qual_fn = nullptr;
          }
          cchar *attr = trailer->children[0]->str_val;
          // issue 027 feature: `ClassName.attr` -- class-qualified
          // member access. When attr names a method-valued prototype
          // field (own or inherited; the has[]/alias convention),
          // resolve by kind:
          //  - @staticmethod: the raw function value, fully resolved
          //    here -- subsequent call/reference is an ordinary value
          //    use (the value convention: as[0] == fn).
          //  - @classmethod: remember fn + class; the PY_call trailer
          //    injects the class VALUE as the first (cls) argument.
          //  - regular method: remember the qualifying class; a
          //    PY_call with an explicit receiver dispatches STATICALLY
          //    as that class via Sym::aspect (the super() mechanism)
          //    while the receiver keeps its concrete type
          //    (`Base.method(self, ...)`, `Base.__init__(self)`).
          // Data attributes (A.n) and non-call member references keep
          // the historical prototype-period path. `->self` is only
          // ever set on a class's own Sym (gen_class_pyda), never an
          // instance variable, so it discriminates literal class
          // references from values.
          if (cur_val && cur_val->self && cur_val->type_kind == Type_RECORD) {
            Sym *mfn = find_class_method_fn(cur_val, attr);
            if (mfn && mfn->is_static_method) {
              cur_val = mfn;
              continue;
            }
            if (mfn) {
              qual_cls = cur_val;
              qual_fn = mfn->is_class_method ? mfn : nullptr;
            }
          }
          pending_sym = make_symbol(attr);
          pending_member = true;
        } else if (trailer->kind == PY_call) {
          // Collect positional and keyword args
          Vec<PyDAST *> pos_args;
          Vec<PyDAST *> kw_keys, kw_vals;
          if (trailer->children.n > 0) {
            PyDAST *arglist = trailer->children[0];
            for (auto arg : arglist->children.values()) {
              if (arg->kind == PY_keyword_arg) {
                kw_keys.add(arg->children[0]);
                kw_vals.add(arg->children[1]);
              } else
                pos_args.add(arg);
            }
          }
          for (auto a : pos_args.values()) build_if1_pyda(a, ctx);
          for (auto a : kw_vals.values()) build_if1_pyda(a, ctx);
          for (auto a : pos_args.values()) if1_gen(if1, &ast->code, getAST(a, ctx)->code);
          for (auto a : kw_vals.values()) if1_gen(if1, &ast->code, getAST(a, ctx)->code);
          // Check for builtin call (only for direct name invocations, no pending member)
          if (!pending_member && build_builtin_call_pyda(atom_ast, trailer, ast, ctx)) {
            cur_val = ast->rval;
            continue;
          }
          // issue 027 feature: @classmethod called through a class --
          // `A.make(v)` (or `B.make(v)` for an inherited make). The
          // dispatch was resolved statically at the attribute trailer;
          // the class VALUE the call was made through becomes the
          // first (cls) argument, so `cls(...)` inside the body
          // constructs the right class via the ordinary
          // __new__-through-meta dispatch.
          if (pending_member && qual_fn) {
            Code *send = if1_send1(if1, &ast->code, ast);
            if1_add_send_arg(if1, send, qual_fn);
            if1_add_send_arg(if1, send, qual_cls);
            for (auto a : pos_args.values()) if1_add_send_arg(if1, send, getAST(a, ctx)->rval);
            for (int ki = 0; ki < kw_vals.n; ki++)
              if1_add_send_arg(if1, send, getAST(kw_vals[ki], ctx)->rval, cannonicalize_string(kw_keys[ki]->str_val));
            ast->rval = new_sym(ast);
            if1_add_send_result(if1, send, ast->rval);
            cur_val = ast->rval;
            pending_member = false;
            qual_cls = qual_fn = nullptr;
            continue;
          }
          Code *send = nullptr;
          int arg_start = 0;
          if (pending_member) {
            Sym *t = new_sym(ast);
            Sym *obj;
            if (qual_cls && pos_args.n >= 1) {
              // issue 027: qualified regular method with an explicit
              // receiver -- `Base.method(recv, ...)`. Static dispatch
              // as Base via Sym::aspect (the mechanism super() uses):
              // the receiver value flows through a temp that
              // masquerades as Base for pattern matching while keeping
              // its concrete type. The aspect here is FINAL -- unlike
              // super()'s, it must NOT be superclass-hopped by
              // fixup_aspect (which now only rewrites the Syms super's
              // lowering registers in super_aspect_syms).
              obj = new_sym(ast);
              obj->aspect = qual_cls;
              if1_move(if1, &ast->code, getAST(pos_args[0], ctx)->rval, obj, ast);
              arg_start = 1;
            } else {
              obj = cur_val->self ? cur_val->self : cur_val;
            }
            Code *op = if1_send(if1, &ast->code, 4, 1, sym_operator, obj, sym_period, pending_sym, t);
            op->ast = ast;
            op->partial = Partial_OK;
            send = if1_send1(if1, &ast->code, ast);
            if1_add_send_arg(if1, send, t);
            pending_member = false;
            qual_cls = qual_fn = nullptr;
          } else {
            send = if1_send1(if1, &ast->code, ast);
            if1_add_send_arg(if1, send, cur_val);
          }
          for (int ai = arg_start; ai < pos_args.n; ai++) if1_add_send_arg(if1, send, getAST(pos_args[ai], ctx)->rval);
          // Keyword arguments: pass each value under its parameter name.
          // The IFA matcher (pattern.cc positional_to_named /
          // named_to_positional) maps these onto the callee's formals by
          // name, so the name string must be interned the same way formal
          // parameter names are (cannonicalize_string == the IF1 string
          // table). Previously kw args were collected and their values
          // built but never added to the send at all -- silently dropped,
          // leaving the callee's params untyped, which surfaced far away
          // as "unresolved call '__gt__'" etc. (issue 025 bucket A;
          // timsort's `sort(low=0, high=len(list_))`). The key node is a
          // bare identifier; only its string is read (never resolved as a
          // variable, which would spuriously globalize it like the
          // attribute-name bug).
          for (int ki = 0; ki < kw_vals.n; ki++)
            if1_add_send_arg(if1, send, getAST(kw_vals[ki], ctx)->rval, cannonicalize_string(kw_keys[ki]->str_val));
          ast->rval = new_sym(ast);
          if1_add_send_result(if1, send, ast->rval);
          cur_val = ast->rval;
        } else if (trailer->kind == PY_subscript) {
          // Flush pending member
          if (pending_member) {
            Sym *t = new_sym(ast);
            Code *send = if1_send(if1, &ast->code, 4, 1, sym_operator, cur_val, sym_period, pending_sym, t);
            send->ast = ast;
            send->partial = Partial_OK;
            cur_val = t;
            pending_member = false;
            qual_cls = qual_fn = nullptr;
          }
          // subscriptlist or single expr
          PyDAST *sub_node = (trailer->children.n > 0) ? trailer->children[0] : nullptr;
          if (!sub_node) break;
          // Handle slice vs index
          if (sub_node->kind == PY_slice) {
            // slice: int_val encodes has_lower(bit0)|has_upper(bit1); last child may be PY_slice (step/sliceop)
            bool has_lower = sub_node->is_int ? (sub_node->int_val & 1) : false;
            bool has_upper = sub_node->is_int ? (sub_node->int_val & 2) : false;
            Sym *l = int64_constant(0), *u = int64_constant(INT_MAX), *s = nullptr;
            int cidx = 0;
            // Check if last child is PY_slice (the sliceop/step)
            bool has_step = (sub_node->children.n > 0 &&
                             sub_node->children[sub_node->children.n-1]->kind == PY_slice);
            int value_children = sub_node->children.n - (has_step ? 1 : 0);
            if (has_step) {
              PyDAST *sliceop = sub_node->children[sub_node->children.n-1];
              if (sliceop->children.n > 0) {
                build_if1_pyda(sliceop->children[0], ctx);
                if1_gen(if1, &ast->code, getAST(sliceop->children[0], ctx)->code);
                s = getAST(sliceop->children[0], ctx)->rval;
              } else
                s = int64_constant(1);
            }
            if (has_lower && cidx < value_children) {
              build_if1_pyda(sub_node->children[cidx], ctx);
              if1_gen(if1, &ast->code, getAST(sub_node->children[cidx], ctx)->code);
              l = getAST(sub_node->children[cidx], ctx)->rval;
              cidx++;
            }
            if (has_upper && cidx < value_children) {
              build_if1_pyda(sub_node->children[cidx], ctx);
              if1_gen(if1, &ast->code, getAST(sub_node->children[cidx], ctx)->code);
              u = getAST(sub_node->children[cidx], ctx)->rval;
            }
            ast->is_object_index = 1;
            bool store = (n->ctx == PY_STORE && i == n->children.n - 1);
            if (!has_step) s = int64_constant(1);
            if (store)
              call_method(&ast->code, ast, cur_val, sym___pyc_setslice__, (ast->rval = new_sym(ast)), 3, l, u, s);
            else
              call_method(&ast->code, ast, cur_val, sym___pyc_getslice__, (ast->rval = new_sym(ast)), 3, l, u, s);
            cur_val = ast->rval;
          } else {
            build_if1_pyda(sub_node, ctx);
            PycAST *sub_ast = getAST(sub_node, ctx);
            if1_gen(if1, &ast->code, sub_ast->code);
            ast->is_object_index = 1;
            if (n->ctx == PY_STORE && i == n->children.n - 1) {
              call_method(&ast->code, ast, cur_val, sym___setitem__, (ast->rval = new_sym(ast)), 1, sub_ast->rval);
            } else {
              call_method(&ast->code, ast, cur_val, sym___getitem__, (ast->rval = new_sym(ast)), 1, sub_ast->rval);
              cur_val = ast->rval;
            }
          }
        } else {
          // '**' factor: flush any pending member, then generate __pow__
          if (pending_member) {
            Sym *obj = cur_val->self ? cur_val->self : cur_val;
            Sym *t = new_sym(ast);
            Code *send = if1_send(if1, &ast->code, 4, 1, sym_operator, obj, sym_period, pending_sym, t);
            send->ast = ast;
            send->partial = Partial_OK;
            cur_val = t;
            pending_member = false;
            qual_cls = qual_fn = nullptr;
          }
          build_if1_pyda(trailer, ctx);
          PycAST *rhs = getAST(trailer, ctx);
          if1_gen(if1, &ast->code, rhs->code);
          Sym *result = new_sym(ast);
          if1_send(if1, &ast->code, 3, 1, make_symbol("__pow__"), cur_val, rhs->rval, result)->ast = ast;
          cur_val = result;
        }
      }
      // Flush final pending member
      if (pending_member) {
        Sym *obj = cur_val->self ? cur_val->self : cur_val;
        if (n->ctx == PY_STORE) {
          ast->is_member = 1;
          ast->sym = pending_sym;
          ast->rval = obj;
        } else {
          Sym *t = new_sym(ast);
          Code *send = if1_send(if1, &ast->code, 4, 1, sym_operator, obj, sym_period, pending_sym, t);
          send->ast = ast;
          send->partial = Partial_OK;
          ast->rval = t;
        }
      } else
        ast->rval = cur_val;
      return 0;
    }

    case PY_name: {
      // rval already set by build_syms_pyda; handle class member access
      TEST_SCOPE printf("%sfound '%s' at level %d\n", ast->sym ? "" : "not ", n->str_val, 0);
      bool load = (n->ctx != PY_STORE);
      Sym *in = ctx.scope_stack[ctx.scope_stack.n - 1]->in;
      if (in && in->type_kind == Type_RECORD && in->has.in(ast->sym)) {
        if (load)
          if1_send(if1, &ast->code, 4, 1, sym_operator, ctx.fun()->self, sym_period,
                   make_symbol(ast->sym->name), (ast->rval = new_sym(ast)))
              ->ast = ast;
        else {
          ast->is_member = 1;
          ast->sym = make_symbol(ast->sym->name);
          ast->rval = ctx.fun()->self;
        }
      } else if (load && ast->sym && ctx.def_internal_fn.get(ast->sym) &&
                 [&] {
                   Sym *ifn = ctx.def_internal_fn.get(ast->sym);
                   for (int i = ctx.scope_stack.n - 1; i >= 0; i--)
                     if (ctx.scope_stack[i]->fun == ifn) return true;
                   return false;
                 }()) {
        // issues/007: a function's own name referenced INSIDE its own
        // body (recursion) resolves to the internal function Sym
        // directly -- see def_internal_fn in python_ifa_int.h. For a
        // CAPTURING nested def (issues/001 carrier class), the
        // callable value is the carrier instance, which inside the
        // body is exactly `self` (fn->self, set before the body walk)
        // -- so a recursive `count(n-1)` becomes a call on self.
        Sym *ifn = ctx.def_internal_fn.get(ast->sym);
        ast->rval = ifn->self ? ifn->self : ifn;
      } else if (load && is_module_data_var(ast->sym)) {
        // ifa/issues/031 step 2: treat a module-level data variable
        // as a memory cell, not a register -- each *read* loads the
        // cell into a fresh local temp. The temp is on FA's
        // first-class track (SSU-renamed, EntrySet-contoured,
        // eligible for issue-025 narrowing and per-read
        // concretization); the cell's shared GLOBAL_CONTOUR AVar
        // keeps the sound flow-insensitive union of all stores.
        // Stores (ctx == PY_STORE) still write the cell directly.
        Sym *t = new_sym(ast);
        if1_move(if1, &ast->code, ast->sym, t, ast);
        ast->rval = t;
      }
      return 0;
    }

    case PY_number:
      ast->rval = make_num_pyda(n, ctx);
      return 0;

    case PY_string: {
      bool is_raw, is_fstring;
      if (n->str_val) skip_string_prefix(n->str_val, &is_raw, &is_fstring);
      else is_fstring = false;
      if (is_fstring)
        build_fstring_pyda(n, ast, ctx);
      else
        ast->rval = eval_string_pyda(n, ctx);
      return 0;
    }

    case PY_list: {
      for (auto c : n->children.values()) {
        build_if1_pyda(c, ctx);
        if1_gen(if1, &ast->code, getAST(c, ctx)->code);
      }
      Code *send = if1_send1(if1, &ast->code, ast);
      if1_add_send_arg(if1, send, sym_primitive);
      if1_add_send_arg(if1, send, sym_make);
      if1_add_send_arg(if1, send, sym_list);
      for (auto c : n->children.values()) if1_add_send_arg(if1, send, getAST(c, ctx)->rval);
      ast->rval = new_sym(ast);
      if1_add_send_result(if1, send, ast->rval);
      return 0;
    }

    case PY_tuple: {
      for (auto c : n->children.values()) {
        build_if1_pyda(c, ctx);
        if1_gen(if1, &ast->code, getAST(c, ctx)->code);
      }
      Code *send = if1_send1(if1, &ast->code, ast);
      if1_add_send_arg(if1, send, sym_primitive);
      if1_add_send_arg(if1, send, sym_make);
      if1_add_send_arg(if1, send, sym_tuple);
      for (auto c : n->children.values()) if1_add_send_arg(if1, send, getAST(c, ctx)->rval);
      ast->rval = new_sym(ast);
      if1_add_send_result(if1, send, ast->rval);
      return 0;
    }

    case PY_star_expr: {
      // issues/024: `*b` inside an assignment-target tuple (`a, *b =
      // ...`). Only meaningful there -- emit_assign_to_target handles
      // the actual binding (a list slice, not a single element) by
      // recognizing this node among a tuple target's children
      // directly, unwrapping to children[0]. This case exists so the
      // ENCLOSING PY_tuple's own build_if1_pyda pass (which walks
      // every child, including this one, to build its -- for a
      // target, unused -- "make tuple from targets" send) has
      // something to build: it just recurses into the real inner
      // target, exactly like a plain (non-star) element would.
      build_if1_pyda(n->children[0], ctx);
      PycAST *inner = getAST(n->children[0], ctx);
      if1_gen(if1, &ast->code, inner->code);
      ast->rval = inner->rval;
      ast->sym = inner->sym;
      return 0;
    }

    case PY_listcomp: {
      // children: [elt_expr, PY_list_for]
      ast->rval = new_sym(ast);
      if1_send(if1, &ast->code, 3, 1, sym_primitive, sym_make, sym_list, ast->rval)->ast = ast;
      reenter_scope_pyda(n, ctx);
      Vec<PyDAST *> elts;
      elts.add(n->children[0]);
      build_list_comp_pyda(n->children[1], elts, ast, &ast->code, ctx);
      exit_scope(ctx);
      return 0;
    }

    case PY_ternary: {
      // children: [body_if_true, condition, else_val]
      build_if1_pyda(n->children[0], ctx);  // body
      build_if1_pyda(n->children[1], ctx);  // cond
      build_if1_pyda(n->children[2], ctx);  // orelse
      gen_ifexpr(getAST(n->children[1], ctx), getAST(n->children[0], ctx), getAST(n->children[2], ctx), ast);
      return 0;
    }

    case PY_testlist:
    case PY_exprlist: {
      // Single child: pass through; multiple: treat as tuple
      if (n->children.n == 1) {
        build_if1_pyda(n->children[0], ctx);
        PycAST *c = getAST(n->children[0], ctx);
        ast->code = c->code;
        ast->rval = c->rval;
        ast->sym = c->sym;
        ast->is_member = c->is_member;
        ast->is_object_index = c->is_object_index;
      } else {
        for (auto c : n->children.values()) {
          build_if1_pyda(c, ctx);
          if1_gen(if1, &ast->code, getAST(c, ctx)->code);
        }
        Code *send = if1_send1(if1, &ast->code, ast);
        if1_add_send_arg(if1, send, sym_primitive);
        if1_add_send_arg(if1, send, sym_make);
        if1_add_send_arg(if1, send, sym_tuple);
        for (auto c : n->children.values()) if1_add_send_arg(if1, send, getAST(c, ctx)->rval);
        ast->rval = new_sym(ast);
        if1_add_send_result(if1, send, ast->rval);
        ast->sym = ast->rval;
      }
      return 0;
    }

    case PY_yield_stmt: {
      // issues/014: `yield expr` as a statement. children: [expr]
      // (testlist) or none (`yield` alone yields None). Lowered via a
      // "yield" primitive send exactly mirroring PY_await_expr's
      // "await" primitive below -- codegen (cg.cc, is_generator)
      // turns it into `co_yield <value>;`. The send's own result slot
      // (whatever a `.send(v)` delivers) genuinely has no consumer
      // here -- `yield foo` as a bare statement discards it just like
      // real Python does; PY_yield_expr below is the form that reads
      // it (`x = yield foo`).
      Sym *yval = sym_nil;
      if (n->children.n > 0) {
        build_if1_pyda(n->children[0], ctx);
        PycAST *val_ast = getAST(n->children[0], ctx);
        if1_gen(if1, &ast->code, val_ast->code);
        yval = val_ast->rval;
      }
      Sym *unused = new_sym(ast);
      Code *send = if1_send(if1, &ast->code, 3, 1, sym_primitive, make_symbol("yield"), yval, unused);
      send->ast = ast;
      return 0;
    }

    case PY_raise_stmt:
    case PY_try_stmt: {
      // Exception handling is unimplemented (issue 011), but a `try`
      // must not crash the compiler. It was routed to the WITH-item
      // handler (build_if1_with_items) along with `with` -- which
      // mis-lowered the try body into null rvals and aborted if1_send
      // (e.g. amaze/go/othello/sudoku3: a method call inside `try`).
      // Build the try body plus the else/finally clauses (the normal
      // control flow) so the happy path compiles and runs; skip the
      // except handlers. Children: body suite, except_handler*,
      // else_clause?, finally_clause?.
      for (PyDAST *c : n->children.values()) {
        if (c->kind == PY_except_handler || c->kind == PY_except_clause) continue;
        // else_clause / finally_clause wrap a suite; build that suite.
        PyDAST *blk = ((c->kind == PY_else_clause || c->kind == PY_finally_clause) && c->children.n)
                          ? c->children.last()
                          : c;
        build_if1_pyda(blk, ctx);
        if1_gen(if1, &ast->code, getAST(blk, ctx)->code);
      }
      return 0;
    }
    case PY_except_clause:
    case PY_except_handler:
    case PY_finally_clause:
      // Reached only if built standalone (the try handler above skips
      // except handlers and inlines finally); no-op defensively.
      return 0;
    case PY_with_stmt: {
      build_if1_with_items(n, 0, ctx, ast);
      return 0;
    }
    case PY_with_item:
      fail("error line %d, statement not supported in pyda path", ctx.lineno);
      return -1;

    case PY_await_expr: {
      build_if1_pyda(n->children[0], ctx);
      PycAST *val_ast = getAST(n->children[0], ctx);
      if1_gen(if1, &ast->code, val_ast->code);
      ast->rval = new_sym(ast);
      Code *send = if1_send(if1, &ast->code, 3, 1, sym_primitive, make_symbol("await"), val_ast->rval, ast->rval);
      send->ast = ast;
      ast->sym = ast->rval;
      return 0;
    }

    case PY_assert_stmt: {
      // children: [cond] or [cond, msg]. No exception model exists yet
      // (issues/011), so this lowers to `if not cond:
      // __pyc_assert_fail__(msg)` -- an abort, not a catchable
      // AssertionError. The message expression is only built inside the
      // false branch (not evaluated eagerly before the check), matching
      // real Python's `assert cond, msg` never evaluating `msg` unless
      // the assertion fails.
      build_if1_pyda(n->children[0], ctx);
      PycAST *cond_ast = getAST(n->children[0], ctx);
      if1_gen(if1, &ast->code, cond_ast->code);
      Sym *cond_bool = new_sym(ast);
      call_method(&ast->code, ast, cond_ast->rval, sym___pyc_to_bool__, cond_bool, 0);
      Label *cont_label = if1_alloc_label(if1);
      Code *ifcode = if1_if_goto(if1, &ast->code, cond_bool, ast);
      if1_if_label_true(if1, ifcode, cont_label);
      if1_if_label_false(if1, ifcode, if1_label(if1, &ast->code, ast));
      Sym *msg_val;
      if (n->children.n > 1) {
        build_if1_pyda(n->children[1], ctx);
        PycAST *msg_ast = getAST(n->children[1], ctx);
        if1_gen(if1, &ast->code, msg_ast->code);
        msg_val = new_sym(ast);
        call_method(&ast->code, ast, msg_ast->rval, sym___str__, msg_val, 0);
      } else {
        msg_val = make_string("");
      }
      PycSymbol *fail_fn = make_PycSymbol(ctx, "__pyc_assert_fail__", PYC_USE);
      if (!fail_fn) fail("error line %d, '__pyc_assert_fail__' not found (is __pyc__/05_builtins.py loaded?)", n->line);
      Code *send = if1_send1(if1, &ast->code, ast);
      if1_add_send_arg(if1, send, fail_fn->sym);
      if1_add_send_arg(if1, send, msg_val);
      if1_add_send_result(if1, send, new_sym(ast));
      if1_label(if1, &ast->code, ast, cont_label);
      return 0;
    }

    // Nodes that are handled by their parent
    case PY_list_for:
    case PY_list_if:
    case PY_comp_for:
    case PY_comp_if:
    case PY_else_clause:
    case PY_fpdef:
    case PY_fplist:
    case PY_varargslist:
    case PY_parameters:
    case PY_arg_default:
    case PY_star_arg:
    case PY_dstar_arg:
    case PY_arglist:
    case PY_keyword_arg:
    case PY_decorator:
    case PY_cmp_op:
      for (auto c : n->children.values()) build_if1_pyda(c, ctx);
      return 0;

    case PY_dict: {
      // Create dict instance by calling dict() constructor
      if (!ast->sym) fail("error line %d, 'dict' type not found (is __pyc__/07_dict.py loaded?)", n->line);
      Code *ctor = if1_send1(if1, &ast->code, ast);
      if1_add_send_arg(if1, ctor, ast->sym);  // dict class sym (set by build_syms_pyda)
      Sym *dict_inst = new_sym(ast);
      ast->rval = dict_inst;
      if1_add_send_result(if1, ctor, dict_inst);
      if (n->children.n == 3 && n->children[2]->kind == PY_comp_for) {
        // Dict comprehension: {key: value for target in iter [if cond]}.
        // build_syms_pyda gave this its own scope (matching
        // PY_listcomp/PY_set); reenter it here the same way PY_listcomp
        // does via reenter_scope_pyda.
        reenter_scope_pyda(n, ctx);
        Vec<PyDAST *> elts;
        elts.add(n->children[0]);
        elts.add(n->children[1]);
        build_list_comp_pyda(n->children[2], elts, ast, &ast->code, ctx, sym___setitem__);
        exit_scope(ctx);
      } else {
        // Flat dict literal: {k1: v1, k2: v2, ...}
        for (int i = 0; i + 1 < n->children.n; i += 2) {
          build_if1_pyda(n->children[i], ctx);
          build_if1_pyda(n->children[i + 1], ctx);
          if1_gen(if1, &ast->code, getAST(n->children[i], ctx)->code);
          if1_gen(if1, &ast->code, getAST(n->children[i + 1], ctx)->code);
        }
        for (int i = 0; i + 1 < n->children.n; i += 2) {
          call_method(&ast->code, ast, dict_inst, sym___setitem__, new_sym(ast), 2,
                      getAST(n->children[i], ctx)->rval,
                      getAST(n->children[i + 1], ctx)->rval);
        }
      }
      return 0;
    }

    case PY_set: {
      // Two grammar shapes (python.g dictorsetmaker): a flat literal
      // `{e1, e2, ...}` (n children, all element exprs), or a set
      // comprehension `{expr for target in iter}` (2 children: [expr,
      // PY_comp_for]) — build_syms_pyda gave the comprehension form its own
      // scope, matching PY_listcomp; reenter it here the same way
      // PY_listcomp does via reenter_scope_pyda.
      if (!ast->sym) fail("error line %d, 'set' type not found (is __pyc__/08_set.py loaded?)", n->line);
      Code *ctor = if1_send1(if1, &ast->code, ast);
      if1_add_send_arg(if1, ctor, ast->sym);  // set class sym (set by build_syms_pyda)
      Sym *set_inst = new_sym(ast);
      ast->rval = set_inst;
      if1_add_send_result(if1, ctor, set_inst);
      if (n->children.n == 2 && n->children[1]->kind == PY_comp_for) {
        reenter_scope_pyda(n, ctx);
        Vec<PyDAST *> elts;
        elts.add(n->children[0]);
        build_list_comp_pyda(n->children[1], elts, ast, &ast->code, ctx, make_symbol("add"));
        exit_scope(ctx);
      } else {
        for (auto c : n->children.values()) {
          build_if1_pyda(c, ctx);
          if1_gen(if1, &ast->code, getAST(c, ctx)->code);
          call_method(&ast->code, ast, set_inst, make_symbol("add"), new_sym(ast), 1, getAST(c, ctx)->rval);
        }
      }
      return 0;
    }

    case PY_genexpr:
      // Generator expressions need a lazily-evaluated iterator object (see
      // issue 014's generator/yield work); until that lands, fail cleanly
      // instead of falling through to the no-op default case (issue 008 —
      // that used to crash the compiler with an internal if1_move
      // assertion rather than reject the input).
      fail("error line %d, generator expressions not yet supported (see issues/008, issues/014)", ctx.lineno);
      return -1;

    case PY_yield_expr: {
      // issues/014: `yield expr` as an expression (`x = yield foo`).
      // Same "yield" primitive send as PY_yield_stmt above, but this
      // form's own value is consumed by the caller -- real Python
      // gives it whatever a subsequent `.send(v)` provides (None for
      // a plain `next()`/`__pyc_more__`-driven advance). cg.cc's
      // P_prim_yield now assigns the send's real result Sym here
      // (`yval_result`) instead of the old hardcoded `sym_nil`, wired
      // through to `co_yield`'s own expression value (see
      // pyc_c_runtime.h's yield_awaiter) -- this is what makes
      // `.send()` observable inside the generator body at all.
      Sym *yval = sym_nil;
      if (n->children.n > 0) {
        build_if1_pyda(n->children[0], ctx);
        PycAST *val_ast = getAST(n->children[0], ctx);
        if1_gen(if1, &ast->code, val_ast->code);
        yval = val_ast->rval;
      }
      Sym *yval_result = new_sym(ast);
      Code *send = if1_send(if1, &ast->code, 3, 1, sym_primitive, make_symbol("yield"), yval, yval_result);
      send->ast = ast;
      ast->rval = yval_result;
      ast->sym = ast->rval;
      return 0;
    }

    case PY_slice:
    case PY_subscriptlist:
    case PY_dotted_name:
    case PY_dotted_as_name:
    case PY_import_as_name:
    default:
      for (auto c : n->children.values()) build_if1_pyda(c, ctx);
      return 0;
  }
}

int build_if1_module_pyda(PyDAST *mod, PycCompiler &ctx, Code **code) {
  // Imports splice the imported module's code into &getAST(ctx.node)->code
  // (see build_import_if1), so ctx.node must be restored on the way out:
  // without this, any import AFTER the first one (later statement or same
  // comma-separated statement) captures the previously imported module's
  // dead PycAST stream and its top-level code is silently dropped.
  void *saved_node = ctx.node;
  ctx.node = mod;
  enter_scope(ctx);  // re-enters module scope created during build_syms
  for (auto c : mod->children.values()) {
    build_if1_pyda(c, ctx);
    if1_gen(if1, code, getAST(c, ctx)->code);
  }
  exit_scope(ctx);
  ctx.node = saved_node;
  return 0;
}
