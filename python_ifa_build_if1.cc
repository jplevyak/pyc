// SPDX-License-Identifier: BSD-3-Clause
#include "python_ifa_int.h"

static int build_if1_pyda(PyDAST *n, PycCompiler &ctx);

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
  assert(m);
  if (!m->built_if1) {
    Code **c;
    c = &getAST((PyDAST *)ctx.node, ctx)->code;
    build_module_attributes_if1(m, *m->ctx, c);
    build_if1_module_pyda(m->pymod, *m->ctx, c);
    m->built_if1 = true;
  }
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
    case PY_OP_DIV: return make_symbol("__div__");
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
    case PY_OP_DIV: return make_symbol("__idiv__");
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
    case PY_CMP_LTGT: return make_symbol("__ne__");
    default: assert(!"unhandled cmpop"); return nullptr;
  }
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
    if (*p == 'l' || *p == 'L') break;  // long suffix, skip
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
static Sym *eval_string_pyda(PyDAST *n, PycCompiler &ctx) {
  const char *s = n->str_val;
  if (!s || !*s) return make_string("");
  // Skip string prefix (r/b/u/R/B/U combinations, e.g. r, rb, br, u, b, etc.)
  bool is_raw = false;
  while (*s && (*s == 'r' || *s == 'R' || *s == 'b' || *s == 'B' || *s == 'u' || *s == 'U')) {
    if (*s == 'r' || *s == 'R') is_raw = true;
    s++;
  }
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
  int content_len = (int)(end - s);
  if (!is_raw) {
    // Process escape sequences — output buffer can be at most content_len chars
    char *out = (char *)MALLOC(content_len + 1);
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
    return make_string(out);
  } else {
    // Raw string: copy as-is
    char *out = (char *)MALLOC(content_len + 1);
    memcpy(out, s, content_len);
    out[content_len] = 0;
    return make_string(out);
  }
}

// Check for and handle builtin function calls (super, __pyc_symbol__, etc.)
static int build_builtin_call_pyda(PycAST *atom_ast, PyDAST *call_trailer, PycAST *ast, PycCompiler &ctx) {
  Sym *f = atom_ast->sym;
  if (!f || !builtin_functions.set_in(f)) return 0;
  // Collect positional args from the call trailer
  Vec<PyDAST *> pos_args;
  if (call_trailer && call_trailer->children.n > 0) {
    PyDAST *arglist = call_trailer->children[0];
    for (auto arg : arglist->children.values())
      if (arg->kind != PY_keyword_arg) pos_args.add(arg);
  }
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
      if1_move(if1, &ast->code, ctx.fun()->self, ast->rval);
    } else if (n == 1) {
      PycAST *cls_ast = getAST(pos_args[0], ctx);
      if (!cls_ast->sym || cls_ast->sym->type_kind != Type_RECORD) fail("non-constant super() class");
      ast->rval = new_sym(ast);
      ast->rval->aspect = cls_ast->sym;
      if1_move(if1, &ast->code, fun_has[0], ast->rval);
    } else {
      if (n > 2) fail("bad number of arguments to builtin function 'super'");
      PycAST *a0 = getAST(pos_args[0], ctx);
      PycAST *a1 = getAST(pos_args[1], ctx);
      if (!a0->sym || a0->sym->type_kind != Type_RECORD) fail("non-constant super() class");
      ast->rval = new_sym(ast);
      ast->rval->aspect = a0->sym;
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
  } else
    fail("unimplemented builtin '%s'", f->name);
  return 1;
}

// Build list comprehension for pyda path
static void build_list_comp_pyda(PyDAST *list_for, PyDAST *elt, PycAST *ast, Code **code, PycCompiler &ctx);

static void build_list_comp_inner_pyda(PyDAST *iter_node, PyDAST *elt, PycAST *ast, Code **code, PycCompiler &ctx) {
  if (!iter_node) {
    // Base case: emit the element
    build_if1_pyda(elt, ctx);
    PycAST *elt_ast = getAST(elt, ctx);
    if1_gen(if1, code, elt_ast->code);
    call_method(code, ast, ast->rval, sym_append, new_sym(ast), 1, elt_ast->rval);
    return;
  }
  if (iter_node->kind == PY_list_for || iter_node->kind == PY_comp_for) {
    build_list_comp_pyda(iter_node, elt, ast, code, ctx);
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
    build_list_comp_inner_pyda(next_iter, elt, ast, code, ctx);
    if1_label(if1, code, ast, short_circuit);
  }
}

static void build_list_comp_pyda(PyDAST *list_for, PyDAST *elt, PycAST *ast, Code **code, PycCompiler &ctx) {
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
  if (t->code) if1_gen(if1, &body, t->code);
  call_method(&body, ast, iter, sym_next, tmp, 0);
  if1_move(if1, &body, tmp, t->sym, ast);
  build_list_comp_inner_pyda(next_iter, elt, ast, &body, ctx);
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
  // or PY_elif_clause: children = [cond, suite]
  PyDAST *cond = n->children[0];
  PyDAST *suite = n->children[1];
  build_if1_pyda(cond, ctx);
  PycAST *cond_ast = getAST(cond, ctx);
  if1_gen(if1, &ast->code, cond_ast->code);
  Sym *t = new_sym(ast);
  call_method(&ast->code, ast, cond_ast->rval, sym___pyc_to_bool__, t, 0);
  // Build "then" code — suite may be PY_suite or direct statement
  Code *then_code = 0;
  build_if1_suite_pyda(suite, &then_code, ctx);
  // Build "else" code (elif chain or else clause)
  Code *else_code = 0;
  for (int i = 2; i < n->children.n; i++) {
    PyDAST *child = n->children[i];
    if (child->kind == PY_elif_clause) {
      PycAST *elif_ast = getAST(child, ctx);
      build_if_pyda(child, elif_ast, ctx);
      if1_gen(if1, &else_code, elif_ast->code);
    } else if (child->kind == PY_else_clause) {
      // else_clause child[0] is the suite
      build_if1_suite_pyda(child->children[0], &else_code, ctx);
    }
  }
  if1_if(if1, &ast->code, 0, t, then_code, 0, else_code, 0, 0, ast);
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
        ast->rval = def_ast->rval;
        ast->code = def_ast->code;
      } else if (def->kind == PY_classdef) {
        PycAST *def_ast = getAST(def, ctx);
        // Detect @vector("s") decorator for builtin classes
        char *vector_size = nullptr;
        if (ctx.is_builtin()) {
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
              if (dec->kind != PY_decorator || dec->children.n < 2) continue;
              PyDAST *fname_node = dec->children[0];
              cchar *fname = (fname_node->kind == PY_dotted_name || fname_node->kind == PY_name)
                              ? fname_node->str_val : nullptr;
              if (!fname || strcmp(fname, "vector") != 0) continue;
              // Found @vector — extract string arg from arglist
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
      PyDAST *varargsl = (n->children.n > 0 && n->children[0]->kind == PY_varargslist) ? n->children[0] : nullptr;
      if (varargsl)
        for (auto c : varargsl->children.values())
          if (c->kind == PY_arg_default) build_if1_pyda(c->children[1], ctx);
      build_if1_pyda(n->children.last(), ctx);
      gen_lambda_pyda(n, ast, ctx);
      exit_scope(ctx);
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
        if1_move(if1, &ast->code, sym_nil, ctx.fun()->ret, ast);
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
        build_if1_pyda(tgt, ctx);
        PycAST *a = getAST(tgt, ctx);
        if (tgt->kind == PY_tuple || tgt->kind == PY_testlist || tgt->kind == PY_exprlist) {
          // Destructuring assignment
          if (!a->sym) fail("error line %d, illegal destructuring", ctx.lineno);
          for (int j = 0; j < tgt->children.n; j++) {
            PycAST *elt = getAST(tgt->children[j], ctx);
            call_method(&ast->code, ast, v->rval, sym___getitem__, elt->rval, 1, int64_constant(j));
          }
        } else {
          if1_gen(if1, &ast->code, a->code);
          if (a->is_member)
            if1_send(if1, &ast->code, 5, 1, sym_operator, a->rval, sym_setter, a->sym, v->rval,
                     (ast->rval = new_sym(ast)))
                ->ast = ast;
          else if (a->is_object_index)
            if1_add_send_arg(if1, find_send(a->code), v->rval);
          else
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

    case PY_print_stmt: {
      for (int i = 0; i < n->children.n; i++) {
        PyDAST *child = n->children[i];
        build_if1_pyda(child, ctx);
        PycAST *a = getAST(child, ctx);
        if1_gen(if1, &ast->code, a->code);
        if (i) if1_send(if1, &ast->code, 3, 1, sym_primitive, sym_write, make_string(" "), new_sym(ast))->ast = ast;
        Sym *t = new_sym(ast);
        call_method(&ast->code, ast, a->rval, sym___str__, t, 0);
        if1_send(if1, &ast->code, 3, 1, sym_primitive, sym_write, t, new_sym(ast))->ast = ast;
      }
      if1_send(if1, &ast->code, 2, 1, sym_primitive, sym_writeln, new_sym(ast))->ast = ast;
      return 0;
    }

    case PY_for_stmt: {
      // children: [target, iter, PY_suite, PY_else_clause?]
      build_if1_pyda(n->children[1], ctx);  // iter
      build_if1_pyda(n->children[0], ctx);  // target
      PycAST *t = getAST(n->children[0], ctx);
      PycAST *i_ast = getAST(n->children[1], ctx);
      Sym *iter = new_sym(ast), *tmp = new_sym(ast), *tmp2 = new_sym(ast);
      if1_gen(if1, &ast->code, i_ast->code);
      if1_gen(if1, &ast->code, t->code);
      if1_send(if1, &ast->code, 2, 1, sym___iter__, i_ast->rval, iter)->ast = ast;
      Code *cond = 0, *body = 0, *orelse = 0, *next = 0;
      call_method(&cond, ast, iter, sym___pyc_more__, tmp, 0);
      call_method(&body, ast, iter, sym_next, tmp2, 0);
      if1_move(if1, &body, tmp2, t->sym, ast);
      build_if1_suite_pyda(n->children[2], &body, ctx);
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
      build_if1_suite_pyda(n->children[1], &body, ctx);
      if (n->children.n > 2 && n->children[2]->kind == PY_else_clause)
        build_if1_suite_pyda(n->children[2]->children[0], &orelse, ctx);
      if1_loop(if1, &ast->code, ast->label[0], ast->label[1], t->rval, 0, t->code, 0, body, ast);
      if1_gen(if1, &ast->code, orelse);
      return 0;
    }

    case PY_if_stmt:
      build_if_pyda(n, ast, ctx);
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
      if (n->children.n == 1 && n->children[0]->kind == PY_dotted_as_name)
        names.add(n->children[0]);
      else
        for (auto c : n->children.values())
          if (c->kind == PY_dotted_as_name) names.add(c);
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
      for (int i = 1; i < n->children.n; i++) {
        PyDAST *child = n->children[i];
        if (child->kind == PY_testlist) {
          for (auto ia : child->children.values())
            if (ia->kind == PY_import_as_name) {
              cchar *sym_name = ia->children[0]->str_val;
              cchar *as_name = (ia->children.n > 1) ? ia->children[1]->str_val : nullptr;
              build_import_if1(const_cast<char *>(sym_name), const_cast<char *>(as_name),
                               const_cast<char *>(from_mod), ctx);
            }
        } else if (child->kind == PY_import_as_name) {
          cchar *sym_name = child->children[0]->str_val;
          cchar *as_name = (child->children.n > 1) ? child->children[1]->str_val : nullptr;
          build_import_if1(const_cast<char *>(sym_name), const_cast<char *>(as_name),
                           const_cast<char *>(from_mod), ctx);
        }
      }
      return 0;
    }

    case PY_global_stmt:
      return 0;

    case PY_pass_stmt:
    case PY_del_stmt:
      return 0;

    case PY_break_stmt:
    case PY_continue_stmt:
      if1_goto(if1, &ast->code, ast->label[0])->ast = ast;
      return 0;

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
      ast->label[0] = if1_alloc_label(if1);
      ast->rval = new_sym(ast);
      for (int i = 0; i < nc - 1; i++) {
        build_if1_pyda(n->children[i], ctx);
        PycAST *v = getAST(n->children[i], ctx);
        if1_gen(if1, &ast->code, v->code);
        if1_move(if1, &ast->code, v->rval, ast->rval);
        Sym *t = new_sym(ast);
        call_method(&ast->code, ast, v->rval, sym___pyc_to_bool__, t, 0);
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
      if1_move(if1, &ast->code, v->rval, ast->rval, ast);
      if1_label(if1, &ast->code, ast, ast->label[0]);
      return 0;
    }

    case PY_bool_not: {
      build_if1_pyda(n->children[0], ctx);
      PycAST *v = getAST(n->children[0], ctx);
      if1_gen(if1, &ast->code, v->code);
      ast->rval = new_sym(ast);
      if1_send(if1, &ast->code, 2, 1, make_symbol("__not__"), v->rval, ast->rval)->ast = ast;
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
        if1_send(if1, &ast->code, 3, 1, map_pyop_to_cmp(n->children[1]->op), lv->rval, rv->rval, ast->rval)->ast = ast;
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
          if1_send(if1, &ast->code, 3, 1, map_pyop_to_cmp(op_node->op), ls, rv->rval, s)->ast = ast;
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
      build_if1_pyda(n->children[0], ctx);
      build_if1_pyda(n->children[1], ctx);
      PycAST *lv = getAST(n->children[0], ctx);
      PycAST *rv = getAST(n->children[1], ctx);
      if1_gen(if1, &ast->code, lv->code);
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
      for (int i = 1; i < n->children.n; i++) {
        PyDAST *trailer = n->children[i];
        if (trailer->kind == PY_attribute) {
          // Flush previous pending member
          if (pending_member) {
            Sym *t = new_sym(ast);
            Sym *obj = cur_val->self ? cur_val->self : cur_val;
            Code *send = if1_send(if1, &ast->code, 4, 1, sym_operator, obj, sym_period, pending_sym, t);
            send->ast = ast;
            send->partial = Partial_OK;
            cur_val = t;
            pending_member = false;
          }
          cchar *attr = trailer->children[0]->str_val;
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
          // Check for builtin call (only for direct name invocations, no pending member)
          if (!pending_member && build_builtin_call_pyda(atom_ast, trailer, ast, ctx)) {
            cur_val = ast->rval;
            continue;
          }
          Code *send = nullptr;
          if (pending_member) {
            Sym *t = new_sym(ast);
            Sym *obj = cur_val->self ? cur_val->self : cur_val;
            Code *op = if1_send(if1, &ast->code, 4, 1, sym_operator, obj, sym_period, pending_sym, t);
            op->ast = ast;
            op->partial = Partial_OK;
            send = if1_send1(if1, &ast->code, ast);
            if1_add_send_arg(if1, send, t);
            pending_member = false;
          } else {
            send = if1_send1(if1, &ast->code, ast);
            if1_add_send_arg(if1, send, cur_val);
          }
          for (auto a : pos_args.values()) if1_add_send_arg(if1, send, getAST(a, ctx)->rval);
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
            if (has_step) {
              if (store)
                call_method(&ast->code, ast, cur_val, sym___pyc_setslice__, (ast->rval = new_sym(ast)), 3, l, u, s);
              else
                call_method(&ast->code, ast, cur_val, sym___pyc_getslice__, (ast->rval = new_sym(ast)), 3, l, u, s);
            } else {
              if (store)
                call_method(&ast->code, ast, cur_val, sym___setslice__, (ast->rval = new_sym(ast)), 2, l, u);
              else
                call_method(&ast->code, ast, cur_val, sym___getslice__, (ast->rval = new_sym(ast)), 2, l, u);
            }
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
      }
      return 0;
    }

    case PY_number:
      ast->rval = make_num_pyda(n, ctx);
      return 0;

    case PY_string:
      ast->rval = eval_string_pyda(n, ctx);
      return 0;

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

    case PY_listcomp: {
      // children: [elt_expr, PY_list_for]
      ast->rval = new_sym(ast);
      if1_send(if1, &ast->code, 3, 1, sym_primitive, sym_make, sym_list, ast->rval)->ast = ast;
      reenter_scope_pyda(n, ctx);
      build_list_comp_pyda(n->children[1], n->children[0], ast, &ast->code, ctx);
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
    case PY_exprlist:
    case PY_testlist1: {
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

    case PY_yield_stmt:
    case PY_raise_stmt:
    case PY_try_stmt:
    case PY_except_clause:
    case PY_except_handler:
    case PY_finally_clause:
    case PY_with_stmt:
    case PY_with_item:
      fail("error line %d, statement not supported in pyda path", ctx.lineno);
      return -1;

    case PY_assert_stmt:
      fail("error line %d, 'assert' not yet supported", ctx.lineno);
      return -1;

    case PY_exec_stmt:
      fail("error line %d, 'exec' not yet supported", ctx.lineno);
      return -1;

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
    case PY_backquote:
    case PY_cmp_op:
    case PY_set:
    case PY_dict:
    case PY_genexpr:
    case PY_yield_expr:
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
  ctx.node = mod;
  enter_scope(ctx);  // re-enters module scope created during build_syms
  for (auto c : mod->children.values()) {
    build_if1_pyda(c, ctx);
    if1_gen(if1, code, getAST(c, ctx)->code);
  }
  exit_scope(ctx);
  return 0;
}
