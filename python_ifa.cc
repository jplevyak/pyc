/*
  Copyright 2008 John Plevyak, All Rights Reserved
*/
#include "defs.h"

/* TODO
   move static variables into an object
 */

#define USE_FLOAT_128

#define OPERATOR_CHAR(_c) \
(((_c > ' ' && _c < '0') || (_c > '9' && _c < 'A') || \
  (_c > 'Z' && _c < 'a') || (_c > 'z')) &&            \
   _c != '_'&& _c != '?' && _c != '$')                \

#define _EXTERN
#define _INIT = NULL
#include "python_ops.h"


class LabelMap : public Map<char *, stmt_ty *> {};

class AnalysisOp : public gc { public:
  char *name;
  char *internal_name;
  PrimitiveTransferFunctionPtr ptfn;
  Prim *prim;

  AnalysisOp(char *aname, char *aninternal_name, PrimitiveTransferFunctionPtr pfn)
    : name(aname), internal_name(aninternal_name), ptfn(pfn), prim(0) {}
  AnalysisOp(char *aname, char *aninternal_name, Prim *ap) 
    : name(aname), internal_name(aninternal_name), ptfn(0), prim(ap) {}
};

class ScopeLookupCache : public Map<char *, Vec<Fun *> *> {};
static ScopeLookupCache universal_lookup_cache;
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
PycAST::source_line() { // TODO: relturn 0 for builtin code
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
  Vec<Fun *> *universal = universal_lookup_cache.get(name);
  if (universal) {
    v->set_union(*universal);
    return v;
  }
  else return NULL;
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
new_primitive_object(Sym *&sym, Sym *sym_type, Symbol *symbol, char *name) {
  if (symbol)
    sym = symmap.get(symbol)->sym;
  new_global_variable(sym, name);
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
}

#if 0
static void
build_builtin_symbols() {
  sym_bool = dtBool->asymbol->sym;

  sym_int8  = dtInt[IF1_INT_TYPE_8]->asymbol->sym;
  sym_int16 = dtInt[IF1_INT_TYPE_16]->asymbol->sym;
  sym_int32 = dtInt[IF1_INT_TYPE_32]->asymbol->sym;
  sym_int64 = dtInt[IF1_INT_TYPE_64]->asymbol->sym;

  sym_uint8  = dtUInt[IF1_INT_TYPE_8]->asymbol->sym;
  sym_uint16 = dtUInt[IF1_INT_TYPE_16]->asymbol->sym;
  sym_uint32 = dtUInt[IF1_INT_TYPE_32]->asymbol->sym;
  sym_uint64 = dtUInt[IF1_INT_TYPE_64]->asymbol->sym;

  sym_float32  = dtFloat[IF1_FLOAT_TYPE_32]->asymbol->sym;
  sym_float64  = dtFloat[IF1_FLOAT_TYPE_64]->asymbol->sym;
  sym_float128 = dtFloat[IF1_FLOAT_TYPE_128]->asymbol->sym;

  sym_complex32  = dtComplex[IF1_FLOAT_TYPE_32]->asymbol->sym;
  sym_complex64  = dtComplex[IF1_FLOAT_TYPE_64]->asymbol->sym;
  sym_complex128 = dtComplex[IF1_FLOAT_TYPE_128]->asymbol->sym;

  sym_string = dtString->asymbol->sym;
  sym_anynum = dtNumeric->asymbol->sym;
  sym_any = dtAny->asymbol->sym; 
  sym_object = dtObject->asymbol->sym; 
  sym_nil_type = dtNil->asymbol->sym;
  sym_unknown_type = dtUnknown->asymbol->sym;
  sym_value = dtValue->asymbol->sym;
  sym_void_type = dtVoid->asymbol->sym;
  sym_closure = dtClosure->asymbol->sym;
  sym_symbol = dtSymbol->asymbol->sym;

  new_lub_type(sym_any, "any", 0);
  new_primitive_type(sym_nil_type, "nil_type");
  new_primitive_type(sym_unknown_type, "unknown_type");
  new_primitive_type(sym_void_type, "void_type");
  new_primitive_type(sym_module, "module");
  new_primitive_type(sym_symbol, "symbol");
  if1_set_symbols_type(if1);
  new_primitive_type(sym_closure, "closure");
  new_primitive_type(sym_continuation, "continuation");
  new_primitive_type(sym_vector, "vector");
  new_primitive_type(sym_void_type, "void");
  if (!sym_object)
    sym_object = new_sym("object", 1);
  sym_object->type_kind = Type_RECORD;
  if1_set_builtin(if1, sym_object, "object");
  new_primitive_type(sym_list, "list");
  new_primitive_type(sym_ref, "ref");
  new_primitive_type(sym_value, "value");
  new_primitive_type(sym_set, "set");

  new_primitive_type(sym_int8,   "int8");
  new_primitive_type(sym_int16,  "int16");
  new_primitive_type(sym_int32,  "int32");
  new_primitive_type(sym_int64,  "int64");
  // new_primitive_type(sym_int128, "int128");

  new_alias_type(sym_int, "int", sym_int64);
  new_primitive_type(sym_true, "true");
  new_primitive_type(sym_false, "false");
  new_primitive_type(sym_bool, "bool");
  sym_true->inherits_add(sym_bool);
  sym_false->inherits_add(sym_bool);

  new_primitive_type(sym_uint8, "uint8");
  new_primitive_type(sym_uint16, "uint16");
  new_primitive_type(sym_uint32, "uint32");
  new_primitive_type(sym_uint64, "uint64");
  new_alias_type(sym_uint, "uint", sym_uint64);

  new_lub_type(sym_anyint, "anyint", 
               sym_int8, sym_int16, sym_int32, sym_int64,
               sym_bool,
               sym_uint8, sym_uint16, sym_uint32, sym_uint64, 
               VARARG_END);

  new_alias_type(sym_size, "size", sym_int64);
  new_alias_type(sym_enum_element, "enum_element", sym_int64);
  new_primitive_type(sym_float32, "float32");
  new_primitive_type(sym_float64, "float64");
  new_primitive_type(sym_float128, "float128");
  new_alias_type(sym_float, "float", sym_float64);
  new_lub_type(sym_anyfloat, "anyfloat", 
               sym_float32, sym_float64, sym_float128, 
               VARARG_END);
  new_primitive_type(sym_complex32, "complex32");
  new_primitive_type(sym_complex64, "complex64");
#ifdef USE_FLOAT_128
  new_primitive_type(sym_complex128, "complex128");
#endif
  new_alias_type(sym_complex, "complex", sym_complex64);

  new_lub_type(sym_anycomplex, "anycomplex", 
               sym_complex32, sym_complex64, 
#ifdef USE_FLOAT_128
               sym_complex128, 
#endif
      VARARG_END);
  new_lub_type(sym_anynum, "anynum", sym_anyint, sym_anyfloat, sym_anycomplex, VARARG_END);
  new_primitive_type(sym_char, "char");
  new_primitive_type(sym_string, "string");
  if (!sym_new_object) {
    sym_new_object = new_sym("new_object", 1);
    if1_set_builtin(if1, sym_new_object, "new_object");
  }

  new_primitive_object(sym_nil, sym_nil_type, gNil, "nil");
  new_primitive_object(sym_unknown, sym_unknown_type, gUnknown, "_unknown");
  new_primitive_object(sym_void, sym_void_type, gVoid, "_void");

  sym_init = new_sym(); // placeholder

  builtin_Symbol(dtUnused, sym_tuple, "tuple");

  // automatic promotions

  sym_bool->specializes.add(sym_uint8);

  sym_uint8->specializes.add(sym_uint16);
  sym_uint16->specializes.add(sym_uint32);
  sym_uint32->specializes.add(sym_uint64);

  sym_uint32->specializes.add(sym_int32);
  sym_uint64->specializes.add(sym_int64);

  sym_int8->specializes.add(sym_int16);
  sym_int16->specializes.add(sym_int32);
  sym_int32->specializes.add(sym_int64);

  sym_int32->specializes.add(sym_float32);
  sym_int64->specializes.add(sym_float64);

  sym_float32->specializes.add(sym_float64);
  sym_float64->specializes.add(sym_float128);

  sym_float32->specializes.add(sym_complex32);
  sym_float64->specializes.add(sym_complex64);
#ifdef USE_FLOAT_128
  sym_float128->specializes.add(sym_complex128);
#endif

  sym_complex32->specializes.add(sym_complex64);
#ifdef USE_FLOAT_128
  sym_complex64->specializes.add(sym_complex128);
#endif

  sym_anynum->specializes.add(sym_string);

  // defined type hierarchy
  
  sym_any->implements.add(sym_unknown_type);
  sym_any->specializes.add(sym_unknown_type);
  sym_object->implements.add(sym_any);
  sym_object->specializes.add(sym_any);
  sym_nil_type->implements.add(sym_object);
  sym_nil_type->specializes.add(sym_object);
  sym_value->implements.add(sym_any);
  sym_value->specializes.add(sym_any);

  make_chapel_meta_type(dtAny);
  sym_anytype = sym_any->meta_type;
  sym_anytype->implements.add(sym_any);
  sym_anytype->specializes.add(sym_any);

  make_chapel_meta_type(dtNil);
  sym_nil_type->implements.add(sym_nil_type->meta_type);
  sym_nil_type->specializes.add(sym_nil_type->meta_type);

  sym_any->is_system_type = 1;
  sym_value->is_system_type = 1;
  sym_object->is_system_type = 1;
  sym_nil_type->is_system_type = 1;
  sym_unknown_type->is_system_type = 1;
  sym_void_type->is_system_type = 1;
  sym_anytype->is_system_type = 1;

#define S(_n) assert(sym_##_n);
#include "builtin_symbols.h"
#undef S
}

static int
is_this_fun(Symbol *f) {
  return !strcmp(f->asymbol->sym->name, "this");
}

static int
is_assign_this_fun(Symbol *f) {
  return !strcmp(f->asymbol->sym->name, "=this");
}

static int
gen_fun(Symbol *f) {
  Sym *fn = f->asymbol->sym;
  PycAST* ast = f->defPoint->ainfo;
  Vec<ArgSymbol *> args;
  Vec<Sym *> out_args;
  for_alist(DefExpr, formal, f->formals) {
    args.add(dynamic_cast<ArgSymbol*>(formal->sym));
  }
  Sym *as[args.n + 4];
  int iarg = 0;
  assert(f->asymbol->sym->name);
  if (is_this_fun(f)) {
    if (is_Sym_OUT(args.v[0]->asymbol->sym))
      out_args.add(args.v[0]->asymbol->sym);
    as[iarg++] = args.v[0]->asymbol->sym;
  } else if (is_assign_this_fun(f)) {
    if (is_Sym_OUT(args.v[0]->asymbol->sym))
      out_args.add(args.v[0]->asymbol->sym);
    as[iarg++] = args.v[0]->asymbol->sym;
  } else {
    int setter = f->asymbol->sym->name[0] == '=' && 
      f->asymbol->sym->name[1] &&
      !OPERATOR_CHAR(f->asymbol->sym->name[1]);
    Sym *s = new_sym(f->asymbol->sym->name + (setter ? 1 : 0));
    s->ast = ast;
    s->must_specialize = make_symbol(s->name);
    as[iarg++] = s;
    if (f->isMethod) {
      // this
      if (args.n) {
        if (is_Sym_OUT(args.v[0]->asymbol->sym))
          out_args.add(args.v[0]->asymbol->sym);
        as[iarg++] = args.v[0]->asymbol->sym;
      }
    }
    if (!f->isMethod) {
      if (args.n) {
        if (is_Sym_OUT(args.v[0]->asymbol->sym))
          out_args.add(args.v[0]->asymbol->sym);
        as[iarg++] = args.v[0]->asymbol->sym;
      }
    }
  }
  for (int i = 1; i < args.n; i++) {
    if (is_Sym_OUT(args.v[i]->asymbol->sym))
      out_args.add(args.v[i]->asymbol->sym);
    as[iarg++] = args.v[i]->asymbol->sym;
  }
  Code *body = 0;
  if1_gen(if1, &body, f->body->ainfo->code);
  if1_move(if1, &body, sym_void, fn->ret, ast);
  if1_label(if1, &body, ast, ast->label[0]);
  Code *c = if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret);
  forv_Sym(r, out_args)
    if1_add_send_arg(if1, c, r);
  c->ast = ast;
  c->partial = Partial_NEVER;
  if1_closure(if1, fn, body, iarg, as);
  fn->ast = ast;
  if (f->_this && f->fnClass != FN_CONSTRUCTOR)
    fn->self = f->_this->asymbol->sym;
  //  fun_where_clause(f, f->whereExpr);
  return 0;
}

static int
init_function(Symbol *f) {
  Sym *s = f->asymbol->sym;
  if (ifa_verbose > 2 && f->name)
    printf("build_functions: %s\n", f->name);
  if (f == chpl_main) {
    if1_set_builtin(if1, s, "init");
    sym_init = s;
  }
  s->cont = new_sym();
  PycAST* ast = f->defPoint->ainfo;
  s->cont->ast = ast;
  s->ret = new_sym();
  s->ret->ast = ast;
  s->labelmap = new LabelMap;
  s->nesting_depth = f->nestingDepth();
  return 0;
}

static int
build_function(Symbol *f) {
  if (define_labels(f->body, f->asymbol->sym->labelmap) < 0) return -1;
  PycAST* ast = f->defPoint->ainfo;
  Label *return_label = ast->label[0] = if1_alloc_label(if1);
  if (resolve_labels(f->body, f->asymbol->sym->labelmap, return_label) < 0) return -1;
  if (gen_if1(f->body) < 0) return -1;
  if (gen_fun(f) < 0) return -1;
  return 0;
}

static void
build_classes(Vec<Stmt *> &syms) {
  Vec<ClassType *> classes;
  forv_Stmt(s, syms)
    if (s->astType == TYPE_CLASS)
      classes.add(dynamic_cast<ClassType*>(s)); 
  if (ifa_verbose > 2)
    printf("build_classes: %d classes\n", classes.n);
  forv_Vec(ClassType, c, classes) {
    Sym *csym = c->asymbol->sym;
    forv_Vec(Symbol, tmp, c->fields)
      csym->has.add(tmp->asymbol->sym);
    forv_Vec(TypeSymbol, tmp, c->types) if (tmp)
      if (tmp->definition->astType == TYPE_USER ||
          tmp->definition->astType == TYPE_VARIABLE)
        csym->has.add(tmp->definition->asymbol->sym);
  }
  build_patterns(syms);
}

static int
build_functions(Vec<Stmt *> &syms) {
  forv_Stmt(s, syms)
    if (s->astType == SYMBOL_FN)
      if (init_function(dynamic_cast<Symbol*>(s)) < 0)
        return -1;
  forv_Stmt(s, syms)
    if (s->astType == SYMBOL_FN)
      if (build_function(dynamic_cast<Symbol*>(s)) < 0)
        return -1;
  return 0;
}

static void
add_to_universal_lookup_cache(char *name, Fun *fun) {
  Vec<Fun *> *v = universal_lookup_cache.get(name);
  if (!v)
    v = new Vec<Fun *>;
  v->add(fun);
  universal_lookup_cache.put(name, v);
}

static int
handle_argument(Sym *s, char *name, Fun *fun, int added, MPosition &p) {
  if (s->is_pattern) {
    p.push(1);
    forv_Sym(ss, s->has) {
      added = handle_argument(ss, name, fun, added, p);
      p.inc();
    }
    p.pop();
  }
  // non-scoped lookup if any parameteter is specialized on a reference type
  // (is dispatched)
  if (!added && s->must_specialize && 
      is_reference_type(SYMBOL(s->must_specialize)))
  {
    add_to_universal_lookup_cache(name, fun);
    added = 1;
  }
  // record default argument positions
  if (SYMBOL(s)) {
    ArgSymbol *symbol = dynamic_cast<ArgSymbol*>(SYMBOL(s));
    if (symbol && symbol->defaultExpr) {
      assert(symbol->defaultExpr->ainfo);
      fun->default_args.put(cannonicalize_mposition(p), symbol->defaultExpr->ainfo);
    }
  }
  return added;
}


#endif

static void 
finalize_function(Fun *fun) {
#if 0
  int added = 0;
  char *name = fun->sym->has.v[0]->name;
  assert(name);
  Symbol *fs = dynamic_cast<Symbol*>(SYMBOL(fun->sym));
  if (fs->hasVarArgs)
    fun->is_varargs = 1;
  if (fs->noParens)
    fun->is_eager = 1;
  else
    if (fs->isMethod && !is_this_fun(fs) && !is_assign_this_fun(fs))
      fun->is_lazy = 1;
  if (fs->isMethod)
    fs->_this->asymbol->sym->is_this = 1;
  // add to dispatch cache
  if (fs->_this) {
    if (is_reference_type(SYMBOL(fs->_this->type))) {
      if (fs->isMethod) {
        add_to_universal_lookup_cache(name, fun);
        added = 1;
      }
    }
  }
#endif
}

void
PycCallbacks::finalize_functions() {
  pdb->fa->array_index_base = 1;
  pdb->fa->tuple_index_base = 1;
  forv_Fun(fun, pdb->funs)
    finalize_function(fun);
}


static void
type_equal_transfer_function(PNode *pn, EntrySet *es) {
  AVar *result = make_AVar(pn->lvals.v[0], es);
  //AVar *type = make_AVar(pn->rvals.v[2], es);
  //AVar *val = make_AVar(pn->rvals.v[3], es);
  update_gen(result, make_abstract_type(sym_bool));
}

static void
array_init_transfer_function(PNode *pn, EntrySet *es) {
  AVar *result = make_AVar(pn->lvals.v[0], es);
  AVar *array = make_AVar(pn->rvals.v[2], es);
  AVar *val = make_AVar(pn->rvals.v[4], es);
  forv_CreationSet(a, array->out->sorted) {
    if (a->sym->element)
      flow_vars(val, get_element_avar(a));
  }
  flow_vars(array, result);
}

static void
array_index_transfer_function(PNode *pn, EntrySet *es) {
  AVar *result = make_AVar(pn->lvals.v[0], es);
  AVar *array = make_AVar(pn->rvals.v[2], es);
  set_container(result, array);
  forv_CreationSet(a, array->out->sorted) {
    if (a->sym->element)
      flow_vars(get_element_avar(a), result);
  }
}

static void
array_set_transfer_function(PNode *pn, EntrySet *es) {
  AVar *result = make_AVar(pn->lvals.v[0], es);
  AVar *array = make_AVar(pn->rvals.v[2], es);
  AVar *val = make_AVar(pn->rvals.v[pn->rvals.n-1], es);
  set_container(result, array);
  forv_CreationSet(a, array->out->sorted) {
    if (a->sym->element)
      flow_vars(val, get_element_avar(a));
  }
  flow_vars(array, result);
}

static void
return_bool_transfer_function(PNode *pn, EntrySet *es) {
  AVar *result = make_AVar(pn->lvals.v[0], es);
  update_gen(result, make_abstract_type(sym_bool));
}

static void
return_int_transfer_function(PNode *pn, EntrySet *es) {
  AVar *result = make_AVar(pn->lvals.v[0], es);
  update_gen(result, make_abstract_type(sym_int));
}

static void
return_float_transfer_function(PNode *pn, EntrySet *es) {
  AVar *result = make_AVar(pn->lvals.v[0], es);
  update_gen(result, make_abstract_type(sym_float));
}

static void
return_void_transfer_function(PNode *pn, EntrySet *es) {
  AVar *result = make_AVar(pn->lvals.v[0], es);
  update_gen(result, make_abstract_type(sym_void));
}

static void
return_string_transfer_function(PNode *pn, EntrySet *es) {
  AVar *result = make_AVar(pn->lvals.v[0], es);
  update_gen(result, make_abstract_type(sym_string));
}

static void
array_pointwise_op(PNode *pn, EntrySet *es) {
  AVar *result = make_AVar(pn->lvals.v[0], es);
  AVar *array = make_AVar(pn->rvals.v[2], es);
  flow_vars(array, result);
}

static void
unimplemented_transfer_function(PNode *pn, EntrySet *es) {
  fail("unimplemented primitive");
}

static void
alloc_transfer_function(PNode *pn, EntrySet *es) {
  AVar *tav = make_AVar(pn->rvals.v[2], es);
  AVar *result = make_AVar(pn->lvals.v[0], es);
  forv_CreationSet(cs, tav->out->sorted) {
    Sym *ts = cs->sym;
    if (ts->is_meta_type) ts = ts->meta_type;
    creation_point(result, ts);
  }
}

static int build_stmts(asdl_seq *stmts);

static void add(asdl_seq *seq, Vec<expr_ty> &exprs) {
  for (int i = 0; i < asdl_seq_LEN(seq); i++)
    exprs.add((expr_ty)asdl_seq_GET(seq, i));
}

static void add(asdl_seq *seq, Vec<stmt_ty> &stmts) {
  for (int i = 0; i < asdl_seq_LEN(seq); i++)
    stmts.add((stmt_ty)asdl_seq_GET(seq, i));
}

static void add(expr_ty e, Vec<expr_ty> &exprs) {
  if (e) exprs.add(e);
}

static void add(stmt_ty s, Vec<stmt_ty> &stmts) {
  if (s) stmts.add(s);
}

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
  if (x->kind == FunctionDef_kind ||
      x->kind == ClassDef_kind)
    return enter_scope((void*)x);
  return 0;
}

static int enter_scope(expr_ty x) {
  if (x->kind == Lambda_kind ||
      x->kind == GeneratorExp_kind)
    return enter_scope((void*)x);
  return 0;
}

static int enter_scope(mod_ty x) {
  return enter_scope((void*)x);
}

static void exit_scope() {
  scope = scope_stack.pop();
}

#define BUILD_RECURSE(_ast, _fn) \
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

static int
build_syms(stmt_ty s) {
  BUILD_RECURSE(s, build_syms);
  switch (s->kind) {
    case FunctionDef_kind: // identifier name, arguments args, stmt* body, expr* decorators
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
  exit_scope();
  return 0;
}

static int
build_syms(expr_ty e) {
  BUILD_RECURSE(e, build_syms);
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
      printf("Name\n"); break;
    case List_kind: // expr* elts, expr_context ctx
      printf("List\n"); break;
    case Tuple_kind: // expr *elts, expr_context ctx
      printf("Tuple\n"); break;
  }
  exit_scope();
  return 0;
}

static int build_syms_stmts(asdl_seq *stmts) {
  for (int i = 0; i < asdl_seq_LEN(stmts); i++)
    if (build_syms((stmt_ty)asdl_seq_GET(stmts, i))) return -1;
  return 0;
}

static int
build_syms(mod_ty mod) {
  enter_scope(mod);
  switch (mod->kind) {
    case Module_kind: return build_syms_stmts(mod->v.Module.body);
    case Expression_kind: return build_syms(mod->v.Expression.body);
    case Interactive_kind: return build_syms_stmts(mod->v.Interactive.body);
    case Suite_kind: assert(!"handled");
  }
  exit_scope();
  return 0;
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
      printf("FunctionDef\n"); break;
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
  return 0;
}

static int
build_if1(expr_ty e) {
  RECURSE(e, build_if1);
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
      printf("Name\n"); break;
    case List_kind: // expr* elts, expr_context ctx
      printf("List\n"); break;
    case Tuple_kind: // expr *elts, expr_context ctx
      printf("Tuple\n"); break;
  }
  return 0;
}

static int build_if1_stmts(asdl_seq *stmts) {
  for (int i = 0; i < asdl_seq_LEN(stmts); i++)
    if (build_if1((stmt_ty)asdl_seq_GET(stmts, i))) return -1;
  return 0;
}

static int
build_if1(mod_ty mod) {
  switch (mod->kind) {
    case Module_kind: return build_if1_stmts(mod->v.Module.body);
    case Expression_kind: return build_if1(mod->v.Expression.body);
    case Interactive_kind: return build_if1_stmts(mod->v.Interactive.body);
    case Suite_kind: assert(!"handled");
  }
  return 0;
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

static AnalysisOp *
S(char *name, PrimitiveTransferFunctionPtr pfn) {
  char internal_name[512];
  strcpy(internal_name, "python_");
  strcat(internal_name, name);
  char *new_name = if1_cannonicalize_string(if1, internal_name);
  pdb->fa->primitive_transfer_functions.put(new_name, new RegisteredPrim(pfn));
  return new AnalysisOp(name, new_name, pfn);
}

static AnalysisOp *
P(char *name, Prim *p) {
  char internal_name[512];
  strcpy(internal_name, "python_");
  strcat(internal_name, name);
  char *new_name = if1_cannonicalize_string(if1, internal_name);
  return new AnalysisOp(name, new_name, p);
}

void
init_python_ifa() {
  ifa_init(new PycCallbacks);
  unimplemented_analysis_op = S("unimplemented", unimplemented_transfer_function);
  return_bool_analysis_op = S("return_bool", return_bool_transfer_function);
  return_int_analysis_op = S("return_int", return_int_transfer_function); 
  return_float_analysis_op = S("return_float", return_float_transfer_function);
  return_void_analysis_op = S("return_void", return_void_transfer_function); 
  return_string_analysis_op = S("return_string", return_string_transfer_function); 
  array_init_analysis_op = S("array_init", array_init_transfer_function);
  array_index_analysis_op = S("array_index", array_index_transfer_function);
  array_set_analysis_op = S("array_set", array_set_transfer_function);
  array_pointwise_op_analysis_op = S("array_pointwise_op", array_pointwise_op);
  unary_minus_analysis_op = P("u-", prim_minus);
  unary_plus_analysis_op = P("u+", prim_plus);
  unary_not_analysis_op = P("u~", prim_not);
  unary_lnot_analysis_op = P("!", prim_lnot);
  add_analysis_op = P("+", prim_add);
  subtract_analysis_op = P("-", prim_subtract);
  mult_analysis_op = P("*", prim_mult);
  div_analysis_op = P("/", prim_div);
  mod_analysis_op = P("%", prim_mod);
  lsh_analysis_op = P("<<", prim_lsh);
  rsh_analysis_op = P(">>", prim_rsh);
  equal_analysis_op = P("==", prim_equal);
  notequal_analysis_op = P("!=", prim_notequal);
  lessorequal_analysis_op = P("<=", prim_lessorequal);
  greaterorequal_analysis_op = P(">=", prim_greaterorequal);
  less_analysis_op = P("<", prim_less);
  greater_analysis_op = P(">", prim_greater);
  and_analysis_op = P("&", prim_and);
  or_analysis_op = P("|", prim_or);
  xor_analysis_op = P("^", prim_xor);
  land_analysis_op = P("&&", prim_land);
  lor_analysis_op = P("||", prim_lor);
  pow_analysis_op = P("**", prim_pow);
  get_member_analysis_op = P(".", prim_period);
  set_member_analysis_op = P(".=", prim_setter);
  type_equal_analysis_op = S("type_equal", type_equal_transfer_function);
  alloc_analysis_op = S("alloc", alloc_transfer_function);
}

