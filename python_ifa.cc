/*
  Copyright 2008 John Plevyak, All Rights Reserved
*/
#include "defs.h"

/* TODO
   move static variables into an object
 */

#define OPERATOR_CHAR(_c) \
(((_c > ' ' && _c < '0') || (_c > '9' && _c < 'A') || \
  (_c > 'Z' && _c < 'a') || (_c > 'z')) &&            \
   _c != '_'&& _c != '?' && _c != '$')                \

static Map<Symbol *, PycSymbol *> symmap;
static Map<stmt_ty, PycAST *> stmtmap;
static Map<expr_ty, PycAST *> exprmap;

static struct symtable *symtab = 0;
static int finalized_symbols = 0;

static inline PycAST *getAST(stmt_ty s) {
  PycAST *ast = stmtmap.get(s);
  if (ast) return ast;
  ast = new PycAST;
  ast->xstmt = s;
  stmtmap.put(s, ast);
  return ast;
}

static inline PycAST *getAST(expr_ty e) {
  PycAST *ast = exprmap.get(e);
  if (ast) return ast;
  ast = new PycAST;
  ast->xexpr = e;
  exprmap.put(e, ast);
  return ast;
}

static inline PycSymbol *getSym(Symbol *s) {
  PycSymbol *symbol = symmap.get(s);
  if (symbol) return symbol;
  symbol = new PycSymbol;
  symbol->symbol = s;
  symmap.put(s, symbol);
  return symbol;
}

PycSymbol::PycSymbol() : symbol(0) {
}

char *
cannonicalize_string(char *s) {
  return if1_cannonicalize_string(if1, s);
}

Sym *
PycSymbol::clone() {
  return copy()->sym;
}

char* 
PycSymbol::pathname() {
  if (symbol && symbol->ste_table->st_filename)
    return (char*)symbol->ste_table->st_filename;
  else
    return 0;
}

int 
PycSymbol::line() {
  if (symbol && symbol->ste_lineno)
    return symbol->ste_lineno;
  else
    return 0;
}

int 
PycSymbol::source_line() { // TODO: return 0 for builtin code
  return line();
}

int 
PycSymbol::ast_id() {
  if (symbol)
    return PyInt_AS_LONG(symbol->ste_id);
  else
    return 0;
}

PycAST::PycAST() : xstmt(0), xexpr(0), code(0), sym(0), rval(0) {
  label[0] = label[1] = 0;
}

char
*PycAST::pathname() { // TODO: dig up filename
  //return xast->filename;
  return "<python file>";
}

int
PycAST::line() {
  return xstmt ? xstmt->lineno : xexpr->lineno;
}

int
PycAST::source_line() { // TODO: return 0 for builtin code
  return line();
}

Sym *
PycAST::symbol() {
  if (rval) return rval;
  return sym;
}

IFAAST *
PycAST::copy_node(ASTCopyContext *context) {
  PycAST *a = new PycAST(*this);
  if (context)
    for (int i = 0; i < a->pnodes.n; i++)
      a->pnodes.v[i] = context->nmap->get(a->pnodes.v[i]);
  return a;
}

IFAAST *
PycAST::copy_tree(ASTCopyContext *context) {
  PycAST *a = (PycAST*)copy_node(context);
  for (int i = 0; i < a->children.n; i++)
    a->children.v[i] = (PycAST*)a->children.v[i]->copy_tree(context);
  return a;
}

Vec<Fun *> *
PycAST::visible_functions(Sym *arg0) {
  Vec<Fun *> *v = 0;
  if (arg0->fun) {
    Fun *f = arg0->fun;
    v = new Vec<Fun *>;
    v->add(f);
    return v;
  }
  char *name = 0;
  if (arg0->is_symbol)
    name = arg0->name;
  else
    name = if1_cannonicalize_string(if1, "this");
  // TODO: finish
#if 0
  SymScope* scope = this->xast->parentScope;
  Vec<Symbol *> fss;
  scope->getVisibleFunctions(&fss, name);
  v = new Vec<Fun *>;
  forv_Vec(Symbol, x, fss)
    v->set_add(x->asymbol->sym->fun);
#endif
  return NULL;
}

static PycSymbol *
new_PycSymbol(char *name) {
  PycSymbol *s = new PycSymbol;
  s->sym = new Sym;
  s->sym->asymbol = s;
  if1_register_sym(if1, s->sym, name);
  return s;
}

static PycSymbol *
new_PycSymbol(Symbol *symbol) {
  assert(symbol);
  char *name = 0;
  PycSymbol *s = symmap.get(symbol);
  if (s)
    return s;
  name = PyString_AS_STRING(symbol->ste_name);
  s = new_PycSymbol(name);
  s->symbol = symbol;
  symmap.put(symbol, s);
  return s;
}

static PycSymbol *
new_PycSymbol(Symbol *symbol, Sym *sym) {
  assert(symbol);
  char *name = 0;
  name = PyString_AS_STRING(symbol->ste_name);
  PycSymbol *s = (PycSymbol*)sym->asymbol;
  if (!s) {
    s = new PycSymbol;
    sym->asymbol = s;
  }
  assert(!s->symbol || s->symbol == symbol);
  assert(!s->sym || s->sym == sym);
  s->sym = sym;
  s->sym->asymbol = s;  
  s->symbol = symbol;
  return s;
}

PycSymbol * 
PycSymbol::copy() {
  PycSymbol *s = new PycSymbol;
  s->sym = sym->copy();
  s->sym->asymbol = s;
  if (s->sym->type_kind != Type_NONE)
    s->sym->type = s->sym;
  return s;
}

Sym *
PycCallbacks::new_Sym(char *name) {
  return new_PycSymbol(name)->sym;
}

static void
finalize_symbols(IF1 *i) {
  for (int x = finalized_symbols; x < i->allsyms.n; x++) {
    Sym *s = i->allsyms.v[x];
    if (s->is_constant || s->is_symbol)
      s->nesting_depth = 0;
    else if (s->type_kind)
      s->nesting_depth = 0;
    compute_type_size(s);
  }
  finalized_symbols = i->allsyms.n;
}

static Sym *
new_sym(char *name = 0, int global = 0) {
  Sym *s = new_PycSymbol(name)->sym;
  if (!global)
    s->nesting_depth = LOCALLY_NESTED;
  return s;
}

static Sym *
make_symbol(char *name) {
  Sym *s = if1_make_symbol(if1, name);
  return s;
}

static void
new_primitive_type(Sym *&sym, char *name) {
  name = if1_cannonicalize_string(if1, name);
  if (!sym)
    sym = new_sym(name, 1);
  else
    sym->name = name;
  sym->type_kind = Type_PRIMITIVE;
  if1_set_builtin(if1, sym, name);
}

static void
new_global_variable(Sym *&sym, char *name) {
  if (!sym)
    sym = new_sym(name, 1);
  sym->nesting_depth = 0;
  if1_set_builtin(if1, sym, name);
}

static void
new_primitive_object(Sym *&sym, char *name, Sym *sym_type, Symbol *symbol = 0) {
  if (symbol)
    sym = symmap.get(symbol)->sym;
  new_global_variable(sym, name);
  sym->type_kind = Type_NONE;
  sym->type = sym_type;
  sym->is_external = 1;
  sym_type->is_unique_type = 1;
}

static void
new_alias_type(Sym *&sym, char *name, Sym *alias) {
  if (!sym)
    sym = new_sym(name, 1);
  sym->type_kind = Type_ALIAS;
  sym->alias = alias;
  if1_set_builtin(if1, sym, name);
}

static void
new_lub_type(Sym *&sym, char *name, ...)  {
  if (!sym)
    sym = new_sym(name, 1);
  sym->type_kind = Type_LUB;
  if1_set_builtin(if1, sym, name);
  va_list ap;
  va_start(ap, name);
  Sym *s = 0;
  do {
    if ((s = va_arg(ap, Sym*)))
      sym->has.add(s);
  } while (s);
  forv_Sym(ss, sym->has)
    ss->inherits_add(sym);
}

static void
builtin_Symbol(Symbol *dt, Sym *&sym, char *name) {
  if (!symmap.get(dt))
    new_PycSymbol(dt);
  sym = symmap.get(dt)->sym;
  if1_set_builtin(if1, sym, name);
  if (!sym->type_kind)
    sym->type_kind = Type_PRIMITIVE;
  assert(symmap.get(dt)->symbol == dt);
}

static void
build_builtin_symbols() {

#define S(_n) new_primitive_type(sym_##_n, #_n);
#include "builtin_symbols.h"
#undef S

  new_lub_type(sym_any, "any", 0);
  new_alias_type(sym_int, "int", sym_int32);
  new_alias_type(sym_uint, "uint", sym_uint32);
  new_alias_type(sym_float, "float", sym_float32);
  sym_object->type_kind = Type_RECORD;
  sym_true->inherits_add(sym_bool);
  sym_false->inherits_add(sym_bool);
  new_lub_type(sym_anyint, "anyint",
               sym_int8, sym_int16, sym_int32, sym_int64,
               sym_bool,
               sym_uint8, sym_uint16, sym_uint32, sym_uint64,
               0);
  new_alias_type(sym_size, "size", sym_int64);
  new_alias_type(sym_enum_element, "enum_element", sym_int64);
  new_lub_type(sym_anycomplex, "anycomplex",
               sym_complex32, sym_complex64);
  new_lub_type(sym_anynum, "anynum", sym_anyint, sym_anyfloat, sym_anycomplex, 0);
  new_primitive_object(sym_nil, "nil", sym_nil_type);
  new_primitive_object(sym_unknown, "unknown", sym_unknown_type);
  new_primitive_object(sym_void, "void", sym_void_type);

  sym_any->implements.add(sym_unknown_type);
  sym_any->specializes.add(sym_unknown_type);
  sym_object->implements.add(sym_any);
  sym_object->specializes.add(sym_any);
  sym_nil_type->implements.add(sym_object);
  sym_nil_type->specializes.add(sym_object);
  sym_value->implements.add(sym_any);
  sym_value->specializes.add(sym_any);

  make_meta_type(sym_any);
  sym_anytype = sym_any->meta_type;
  sym_anytype->implements.add(sym_any);
  sym_anytype->specializes.add(sym_any);

  make_meta_type(sym_nil_type);
  sym_nil_type->implements.add(sym_nil_type->meta_type);
  sym_nil_type->specializes.add(sym_nil_type->meta_type);

  sym_any->is_system_type = 1;
  sym_anytype->is_system_type = 1;
  sym_value->is_system_type = 1;
  sym_object->is_system_type = 1;
  sym_nil_type->is_system_type = 1;
  sym_unknown_type->is_system_type = 1;
  sym_void_type->is_system_type = 1;
  
  if1_set_symbols_type(if1);
}

static void 
finalize_function(Fun *fun) {
  fun->is_eager = 1;
}

void
PycCallbacks::finalize_functions() {
  forv_Fun(fun, pdb->funs)
    finalize_function(fun);
}

static void add(asdl_seq *seq, Vec<expr_ty> &exprs) {
  for (int i = 0; i < asdl_seq_LEN(seq); i++)
    exprs.add((expr_ty)asdl_seq_GET(seq, i));
}

static void add(asdl_seq *seq, Vec<stmt_ty> &stmts) {
  for (int i = 0; i < asdl_seq_LEN(seq); i++)
    stmts.add((stmt_ty)asdl_seq_GET(seq, i));
}

static void add(expr_ty e, Vec<expr_ty> &exprs) { if (e) exprs.add(e); }
//static void add(stmt_ty s, Vec<stmt_ty> &stmts) { if (s) stmts.add(s); } unused

static void add_comprehension(asdl_seq *comp, Vec<expr_ty> &exprs) {
  for (int i = 0; i < asdl_seq_LEN(comp); i++) {
    comprehension_ty c = (comprehension_ty)asdl_seq_GET(comp, i);
    add(c->target, exprs);
    add(c->iter, exprs);
    add(c->ifs, exprs);
  }
}

static void get_pre_scope_next(stmt_ty s, Vec<stmt_ty> &stmts, Vec<expr_ty> &exprs) {
  switch (s->kind) {
    default: 
      break;
    case FunctionDef_kind:
      add(s->v.FunctionDef.args->defaults, exprs);
      add(s->v.FunctionDef.decorators, exprs);
      break;
    case ClassDef_kind:
      add(s->v.ClassDef.bases, exprs);
      break;
  }
}

static void get_next(stmt_ty s, Vec<stmt_ty> &stmts, Vec<expr_ty> &exprs) {
  switch (s->kind) {
    default: assert(!"case");
    case FunctionDef_kind:
      add(s->v.FunctionDef.args->args, exprs);
      add(s->v.FunctionDef.body, stmts);
      break;
    case ClassDef_kind:
      add(s->v.ClassDef.body, stmts);
      break;
    case Return_kind: add(s->v.Return.value, exprs); break;
    case Delete_kind: add(s->v.Delete.targets, exprs); break;
    case Assign_kind: add(s->v.Assign.targets, exprs); add(s->v.Assign.value, exprs); break;
    case AugAssign_kind: add(s->v.AugAssign.target, exprs); add(s->v.AugAssign.value, exprs); break;
    case Print_kind: add(s->v.Print.dest, exprs); add(s->v.Print.values, exprs); break;
    case For_kind:
      add(s->v.For.target, exprs); 
      add(s->v.For.iter, exprs); 
      add(s->v.For.body, stmts); 
      add(s->v.For.orelse, stmts); 
      break;
    case While_kind:
      add(s->v.While.test, exprs); 
      add(s->v.While.body, stmts); 
      add(s->v.While.orelse, stmts); 
      break;
    case If_kind:
      add(s->v.If.test, exprs); 
      add(s->v.If.body, stmts); 
      add(s->v.If.orelse, stmts); 
      break;
    case With_kind:
      add(s->v.With.context_expr, exprs); 
      add(s->v.With.optional_vars, exprs); 
      add(s->v.With.body, stmts); 
      break;
    case Raise_kind:
      add(s->v.Raise.type, exprs); 
      add(s->v.Raise.inst, exprs); 
      add(s->v.Raise.tback, exprs); 
      break;
    case TryExcept_kind: {
      add(s->v.TryExcept.body, stmts); 
      for (int i = 0; i < asdl_seq_LEN(s->v.TryExcept.handlers); i++) {
        excepthandler_ty h = (excepthandler_ty)asdl_seq_GET(s->v.TryExcept.handlers, i);
        add(h->type, exprs);
        add(h->name, exprs);
        add(h->body, stmts);
      }
      add(s->v.TryExcept.orelse, stmts); 
      break;
    }
    case TryFinally_kind: add(s->v.TryFinally.body, stmts); add(s->v.TryFinally.finalbody, stmts); break;
    case Assert_kind: add(s->v.Assert.test, exprs); add(s->v.Assert.msg, exprs); break;
    case Import_kind: break;
    case ImportFrom_kind: break;
    case Exec_kind: 
      add(s->v.Exec.body, exprs); 
      add(s->v.Exec.globals, exprs); 
      add(s->v.Exec.locals, exprs); 
      break;
    case Global_kind: break;
    case Expr_kind: add(s->v.Expr.value, exprs); break;
    case Pass_kind:
    case Break_kind:
    case Continue_kind:
      break;
  }
}

static void get_next(slice_ty s, Vec<stmt_ty> &stmts, Vec<expr_ty> &exprs) {
  switch (s->kind) {
    default: assert(!"case");
    case Ellipsis_kind: break;
    case Slice_kind: // (expr? lower, expr? upper, expr? step)
      add(s->v.Slice.lower, exprs); 
      add(s->v.Slice.upper, exprs); 
      add(s->v.Slice.step, exprs); 
      break;
    case ExtSlice_kind: // (slice* dims)
      for (int i = 0; i < asdl_seq_LEN(s->v.ExtSlice.dims); i++)
        get_next((slice_ty)asdl_seq_GET(s->v.ExtSlice.dims, i), stmts, exprs);
      break;
    case Index_kind: // (expr value)
      add(s->v.Index.value, exprs); break;
  }
}

static void get_pre_scope_next(expr_ty e, Vec<stmt_ty> &stmts, Vec<expr_ty> &exprs) {
  switch (e->kind) {
    default: 
      break;
    case Lambda_kind:
      add(e->v.Lambda.args->defaults, exprs);
      break;
  }
}

static void get_next(expr_ty e, Vec<stmt_ty> &stmts, Vec<expr_ty> &exprs) {
  switch (e->kind) {
    default: assert(!"case");
    case BoolOp_kind: // boolop op, expr* values
      add(e->v.BoolOp.values, exprs); break;
    case BinOp_kind: // expr left, operator op, expr right
      add(e->v.BinOp.left, exprs);
      add(e->v.BinOp.right, exprs);
      break;
    case UnaryOp_kind: // unaryop op, expr operand
      add(e->v.UnaryOp.operand, exprs); break;
    case Lambda_kind: // arguments args, expr body
      add(e->v.Lambda.args->args, exprs);
      add(e->v.Lambda.body, exprs); 
      break;
    case IfExp_kind: // expr test, expr body, expr orelse
      add(e->v.IfExp.test, exprs); 
      add(e->v.IfExp.body, exprs); 
      add(e->v.IfExp.orelse, exprs); 
      break;
    case Dict_kind: // expr* keys, expr* values
      add(e->v.Dict.keys, exprs); 
      add(e->v.Dict.values, exprs); 
      break;
    case ListComp_kind: // expr elt, comprehension* generators
      add(e->v.ListComp.elt, exprs); 
      add_comprehension(e->v.ListComp.generators, exprs);
      break;
    case GeneratorExp_kind: // expr elt, comprehension* generators
      add(e->v.GeneratorExp.elt, exprs); 
      add_comprehension(e->v.GeneratorExp.generators, exprs);
      break;
    case Yield_kind: // expr? value
      add(e->v.Yield.value, exprs); break;
    case Compare_kind: // expr left, cmpop* ops, expr* comparators
      add(e->v.Compare.left, exprs);
      add(e->v.Compare.comparators, exprs);
      break;
    case Call_kind: // expr func, expr* args, keyword* keywords, expr? starargs, expr? kwargs
    case Repr_kind: // expr value
     add(e->v.Repr.value, exprs); break;
    case Num_kind: // object n) -- a number as a PyObject
    case Str_kind: // string s) -- need to specify raw, unicode, etc
      break;
    case Attribute_kind: // expr value, identifier attr, expr_context ctx
      add(e->v.Attribute.value, exprs); break;
    case Subscript_kind: // expr value, slice slice, expr_context ctx
     add(e->v.Subscript.value, exprs); 
     get_next(e->v.Subscript.slice, stmts, exprs);
     break;
    case Name_kind: // identifier id, expr_context ctx
      break;
    case List_kind: // expr* elts, expr_context ctx
     add(e->v.List.elts, exprs); break;
    case Tuple_kind: // expr *elts, expr_context ctx
     add(e->v.Tuple.elts, exprs); break;
  }
}

struct PycScope : public gc {
  Symbol *ste;
  PyObject *u_private;
  Map<char *, PycSymbol*> map;
};

static PycScope *scope = 0;
static Vec<PycScope *> scope_stack;

static int enter_scope(void *key) {
  if (scope) scope_stack.add(scope);
  scope = new PycScope;
  scope->ste = PySymtable_Lookup(symtab, key);
  if (scope_stack.n) 
    scope->u_private = scope_stack.last()->u_private;
  else
    scope->u_private = 0;
  assert(scope->ste);
  return 1;
}

static int enter_scope(stmt_ty x) {
  if (x->kind == FunctionDef_kind || x->kind == ClassDef_kind)
    return enter_scope((void*)x);
  return 0;
}

static int enter_scope(expr_ty x) {
  if (x->kind == Lambda_kind || x->kind == GeneratorExp_kind)
    return enter_scope((void*)x);
  return 0;
}

static int enter_scope(mod_ty x) { return enter_scope((void*)x); }
static void exit_scope() { scope = scope_stack.pop(); }
static void exit_scope(mod_ty x) { exit_scope(); }

static void exit_scope(stmt_ty x) {
  if (x->kind == FunctionDef_kind || x->kind == ClassDef_kind)
    exit_scope();
}

static void exit_scope(expr_ty x) {
  if (x->kind == Lambda_kind || x->kind == GeneratorExp_kind)
    exit_scope();
}

#define AST_RECURSE(_ast, _fn) \
  PycAST *ast = getAST(_ast); \
  {                                                                       \
    Vec<stmt_ty> stmts; Vec<expr_ty> exprs; get_pre_scope_next(_ast, stmts, exprs); \
    for_Vec(stmt_ty, x, stmts) { _fn(x); ast->pre_scope_children.add(getAST(x)); }  \
    for_Vec(expr_ty, x, exprs) { _fn(x); ast->pre_scope_children.add(getAST(x)); }  \
  } \
  enter_scope(_ast); \
  {                                                                       \
    Vec<stmt_ty> stmts; Vec<expr_ty> exprs; get_next(_ast, stmts, exprs); \
    for_Vec(stmt_ty, x, stmts) { _fn(x); ast->children.add(getAST(x)); }  \
    for_Vec(expr_ty, x, exprs) { _fn(x); ast->children.add(getAST(x)); }  \
  } \

static int build_syms(stmt_ty s);
static int build_syms(expr_ty e);

// Python name resolution is odd. It is flow sensitive.
// Rather than implement it as flow sensitive, I will convert it
// to static and produce a warning.
Sym *resolve_Sym(PyObject *name) {
  Sym *sym = 0;
  PyObject *mangled = _Py_Mangle(scope->u_private, name);
  (void)mangled;
  int ty = PyST_GetScope(scope->ste, mangled);
  switch(ty) {
    case PYTHON_FREE: // LOAD_DEREF from u_freevars
      printf("PYTHON_FREE\n");
    case CELL: // LOAD_DEREF from u_cellvars
      printf("CELL\n");
    case LOCAL: // if c->u->u_ste->ste_type == FunctionBlock FAST else NAME
      printf("LOCAL ");
      if (scope->ste->ste_type != FunctionBlock)
        goto Lname;
      printf("\n");
    case GLOBAL_IMPLICIT: // if function && optimized GLOBAL else NAME
      printf("GLOBAL_IMPLICIT ");
      if (scope->ste->ste_type != FunctionBlock || scope->ste->ste_unoptimized)
        goto Lname;
      // fall through
    case GLOBAL_EXPLICIT: // GLOBAL
      printf("GLOBAL\n");
      break;
    default: // LOAD_NAME
      // try local first, then global
    Lname:;
      printf("LOAD_NAME\n");
  }
  Py_DECREF(mangled);
  return sym;
}

static int
build_syms(stmt_ty s) {
  AST_RECURSE(s, build_syms);
  switch (s->kind) {
    case FunctionDef_kind: // identifier name, arguments args, stmt* body, expr* decorators
      printf("FunctionDef %s\n", PyString_AsString(s->v.FunctionDef.name));
      resolve_Sym(s->v.FunctionDef.name);
      break;
    case ClassDef_kind: // identifier name, expr* bases, stmt* body
      printf("ClassDef\n"); break;
    case Return_kind: // expr? value
      printf("Return\n"); break;
    case Delete_kind: // expr * targets
      printf("Delete\n"); break;
    case Assign_kind: // expr* targets, expr value
      printf("Assign\n"); break;
    case AugAssign_kind: // expr target, operator op, expr value
      printf("AugAssign\n"); break;
    case Print_kind: // epxr? dest, expr *values, bool nl
      printf("Print\n"); break;
    case For_kind: // expr target, expr, iter, stmt* body, stmt* orelse
      printf("For\n"); break;
    case While_kind: // expr test, stmt*body, stmt*orelse
      printf("While\n"); break;
    case If_kind: // expr tet, stmt* body, stmt* orelse
      printf("If\n"); break;
    case With_kind: // expr content_expr, expr? optional_vars, stmt *body
      printf("With\n"); break;
    case Raise_kind: // expr? type, expr? int, expr? tback
      printf("Raise\n"); break;
    case TryExcept_kind: // stmt* body, excepthandler *handlers, stmt *orelse
      printf("TryExcept\n"); break;
    case TryFinally_kind: // stmt *body, stmt *finalbody
      printf("TryFinally\n"); break;
    case Assert_kind: // expr test, expr? msg
      printf("Assert\n"); break;
    case Import_kind: // alias* name
      printf("Import\n"); break;
    case ImportFrom_kind: // identifier module, alias *names, int? level
      printf("ImportFrom\n"); break;
    case Exec_kind: // expr body, expr? globals, expr? locals
      printf("Exec\n"); break;
    case Global_kind: // identifier* names
      printf("Global\n"); break;
    case Expr_kind: // expr value
      printf("Expr\n"); break;
    case Pass_kind:
      printf("Pass\n"); break;
    case Break_kind:
      printf("Break\n"); break;
    case Continue_kind:
      printf("Continue\n"); break;
      break;
  }
  exit_scope(s);
  return 0;
}

static int
build_syms(expr_ty e) {
  AST_RECURSE(e, build_syms);
  switch (e->kind) {
    case BoolOp_kind: // boolop op, expr* values
      printf("BoolOp\n"); break;
    case BinOp_kind: // expr left, operator op, expr right
      printf("BinOp\n"); break;
    case UnaryOp_kind: // unaryop op, expr operand
      printf("UnaryOp\n"); break;
    case Lambda_kind: // arguments args, expr body
      printf("Lambda\n"); break;
    case IfExp_kind: // expr test, expr body, expr orelse
      printf("IfExp\n"); break;
    case Dict_kind: // expr* keys, expr* values
      printf("Dict\n"); break;
    case ListComp_kind: // expr elt, comprehension* generators
      printf("ListComp\n"); break;
    case GeneratorExp_kind: // expr elt, comprehension* generators
      printf("GeneratorExp\n"); break;
    case Yield_kind: // expr? value
      printf("Yield\n"); break;
    case Compare_kind: // expr left, cmpop* ops, expr* comparators
      printf("Compare\n"); break;
    case Call_kind: // expr func, expr* args, keyword* keywords, expr? starargs, expr? kwargs
      printf("Call\n"); break;
    case Repr_kind: // expr value
      printf("Repr\n"); break;
    case Num_kind: // object n) -- a number as a PyObject
      printf("Num\n"); break;
    case Str_kind: // string s) -- need to specify raw, unicode, etc
      printf("Str\n"); break;
    case Attribute_kind: // expr value, identifier attr, expr_context ctx
      printf("Attribute\n"); break;
    case Subscript_kind: // expr value, slice slice, expr_context ctx
      printf("Subscript\n"); break;
    case Name_kind: // identifier id, expr_context ctx
      printf("Name %s\n", PyString_AsString(e->v.Name.id));
      resolve_Sym(e->v.Name.id);
      break;
    case List_kind: // expr* elts, expr_context ctx
      printf("List\n"); break;
    case Tuple_kind: // expr *elts, expr_context ctx
      printf("Tuple\n"); break;
  }
  exit_scope(e);
  return 0;
}

static int build_syms_stmts(asdl_seq *stmts) {
  for (int i = 0; i < asdl_seq_LEN(stmts); i++)
    if (build_syms((stmt_ty)asdl_seq_GET(stmts, i))) return -1;
  return 0;
}

static int
build_syms(mod_ty mod) {
  int r = 0;
  enter_scope(mod);
  switch (mod->kind) {
    case Module_kind: r = build_syms_stmts(mod->v.Module.body); break;
    case Expression_kind: r = build_syms(mod->v.Expression.body); break;
    case Interactive_kind: r = build_syms_stmts(mod->v.Interactive.body); break;
    case Suite_kind: assert(!"handled");
  }
  exit_scope(mod);
  return r;
}

#define RECURSE(_ast, _fn) \
  PycAST *ast = getAST(_ast); \
  forv_Vec(PycAST, x, ast->pre_scope_children) \
    if (x->xstmt) _fn(x->xstmt); else if (x->xexpr) _fn(x->xexpr); \
  enter_scope(_ast); \
  forv_Vec(PycAST, x, ast->children) \
    if (x->xstmt) _fn(x->xstmt); else if (x->xexpr) _fn(x->xexpr);

static int build_if1(stmt_ty s);
static int build_if1(expr_ty e);

static int
build_if1(stmt_ty s) {
  RECURSE(s, build_if1);
  switch (s->kind) {
    case FunctionDef_kind: // identifier name, arguments args, stmt* body, expr* decorators
      break;
    case ClassDef_kind: // identifier name, expr* bases, stmt* body
      break;
    case Return_kind: // expr? value
      break;
    case Delete_kind: // expr * targets
      break;
    case Assign_kind: // expr* targets, expr value
      break;
    case AugAssign_kind: // expr target, operator op, expr value
      break;
    case Print_kind: // epxr? dest, expr *values, bool nl
      break;
    case For_kind: // expr target, expr, iter, stmt* body, stmt* orelse
      break;
    case While_kind: // expr test, stmt*body, stmt*orelse
      break;
    case If_kind: // expr tet, stmt* body, stmt* orelse
      break;
    case With_kind: // expr content_expr, expr? optional_vars, stmt *body
      break;
    case Raise_kind: // expr? type, expr? int, expr? tback
      break;
    case TryExcept_kind: // stmt* body, excepthandler *handlers, stmt *orelse
      break;
    case TryFinally_kind: // stmt *body, stmt *finalbody
      break;
    case Assert_kind: // expr test, expr? msg
      break;
    case Import_kind: // alias* name
      break;
    case ImportFrom_kind: // identifier module, alias *names, int? level
      break;
    case Exec_kind: // expr body, expr? globals, expr? locals
      break;
    case Global_kind: // identifier* names
      break;
    case Expr_kind: // expr value
      break;
    case Pass_kind:
      break;
    case Break_kind:
      break;
    case Continue_kind:
      break;
  }
  exit_scope(s);
  return 0;
}

static int
build_if1(expr_ty e) {
  RECURSE(e, build_if1);
  switch (e->kind) {
    case BoolOp_kind: // boolop op, expr* values
      break;
    case BinOp_kind: // expr left, operator op, expr right
      break;
    case UnaryOp_kind: // unaryop op, expr operand
      break;
    case Lambda_kind: // arguments args, expr body
      break;
    case IfExp_kind: // expr test, expr body, expr orelse
      break;
    case Dict_kind: // expr* keys, expr* values
      break;
    case ListComp_kind: // expr elt, comprehension* generators
      break;
    case GeneratorExp_kind: // expr elt, comprehension* generators
      break;
    case Yield_kind: // expr? value
      break;
    case Compare_kind: // expr left, cmpop* ops, expr* comparators
      break;
    case Call_kind: // expr func, expr* args, keyword* keywords, expr? starargs, expr? kwargs
      break;
    case Repr_kind: // expr value
      break;
    case Num_kind: // object n) -- a number as a PyObject
      break;
    case Str_kind: // string s) -- need to specify raw, unicode, etc
      break;
    case Attribute_kind: // expr value, identifier attr, expr_context ctx
      break;
    case Subscript_kind: // expr value, slice slice, expr_context ctx
      break;
    case Name_kind: // identifier id, expr_context ctx
      break;
    case List_kind: // expr* elts, expr_context ctx
      break;
    case Tuple_kind: // expr *elts, expr_context ctx
      break;
  }
  exit_scope(e);
  return 0;
}

static int build_if1_stmts(asdl_seq *stmts) {
  for (int i = 0; i < asdl_seq_LEN(stmts); i++)
    if (build_if1((stmt_ty)asdl_seq_GET(stmts, i))) return -1;
  return 0;
}

static int
build_if1(mod_ty mod) {
  int r = 0;
  enter_scope(mod);
  switch (mod->kind) {
    case Module_kind: r = build_if1_stmts(mod->v.Module.body); break;
    case Expression_kind: r = build_if1(mod->v.Expression.body); break;
    case Interactive_kind: r = build_if1_stmts(mod->v.Interactive.body); break;
    case Suite_kind: assert(!"handled");
  }
  exit_scope(mod);
  return r;
}

int 
ast_to_if1(mod_ty module, struct symtable *asymtab) {
  ifa_init(new PycCallbacks);
  symtab = asymtab;
  if (build_syms(module) < 0) return -1;
  build_builtin_symbols();
  //Vec<Symbol *> types;
  //build_types(syms, &types);
  //build_symbols(syms);
  if1_set_primitive_types(if1);
  //build_classes(syms);
  finalize_types(if1, false);
  if (build_if1(module) < 0) return -1;
  //if (build_functions(syms) < 0) return -1;
  finalize_symbols(if1);
  //build_type_hierarchy();
  finalize_types(if1, false);  // again to catch any new ones
  return 0;
}
