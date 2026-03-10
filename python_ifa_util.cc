/*
  Copyright 2008-2011 John Plevyak, All Rights Reserved
*/
#include "python_ifa_int.h"

/* TODO
   move static variables into an object
   "__bases__" "__class__" "lambda"
   decorators (functions applied to functions)
   division and floor division correctly
   Eq and Is correctly
   exceptions
   __radd__ etc. see __pyc__.py
*/

int scope_id = 0;

// -- Globals --
Map<stmt_ty, PycAST *> stmtmap;
Map<expr_ty, PycAST *> exprmap;
Map<PyDAST *, PycAST *> pydmap;
Sym *sym_long = 0, *sym_ellipsis = 0, *sym_ellipsis_type = 0, *sym_unicode = 0, *sym_buffer = 0, *sym_xrange = 0,
    *sym_declare = 0;

#define S(_x) Sym *sym_##_x = 0;
#include "pyc_symbols.h"

cchar *cannonical_self = 0;
int finalized_aspect = 0;
Vec<Sym *> builtin_functions;

PycCallbacks::~PycCallbacks() {}

PycSymbol::PycSymbol() : symbol(0), filename(0), previous(0) {}

void PycCompiler::init() {
  lineno = -1;
  node = 0;
  mod = package = 0;
  modules = 0;
  search_path = 0;
  arena = 0;
}

cchar *cannonicalize_string(cchar *s) { return if1_cannonicalize_string(if1, s); }

Sym *PycSymbol::clone() { return copy()->sym; }

cchar *PycSymbol::pathname() {
  if (filename) return filename;
  if (sym->ast) return sym->ast->pathname();
  return 0;
}

int PycSymbol::column() {
  PycAST *a = (PycAST *)sym->ast;
  if (!a) return 0;
  if (a->xstmt) return a->xstmt->lineno;
  if (a->xexpr) return a->xexpr->lineno;
  if (a->xpyd) return a->xpyd->line;
  return 0;
}

int PycSymbol::line() {
  PycAST *a = (PycAST *)sym->ast;
  if (!a) return 0;
  if (a->xstmt) return a->xstmt->lineno;
  if (a->xexpr) return a->xexpr->lineno;
  if (a->xpyd) return a->xpyd->line;
  return 0;
}

int PycSymbol::source_line() {
  if (sym->ast /* && !((PycAST*)sym->ast)->is_builtin */)  // print them all for now
    return line();
  else
    return 0;
}

int PycSymbol::ast_id() { return 0; }

PycAST::PycAST()
    : xstmt(0),
      xexpr(0),
      xpyd(0),
      filename(0),
      parent(0),
      code(0),
      sym(0),
      rval(0),
      is_builtin(0),
      is_member(0),
      is_object_index(0) {
  label[0] = label[1] = 0;
}

cchar *PycAST::pathname() { return filename; }

int PycAST::column() {
  if (xstmt) return xstmt->col_offset;
  if (xexpr) return xexpr->col_offset;
  if (xpyd) return 0;
  return 0;
}

int PycAST::line() {
  if (xstmt) return xstmt->lineno;
  if (xexpr) return xexpr->lineno;
  if (xpyd) return xpyd->line;
  return 0;
}

int PycAST::source_line() {
  if (is_builtin) return 0;
  return line();
}

Sym *PycAST::symbol() {
  if (rval) return rval;
  return sym;
}

IFAAST *PycAST::copy_node(ASTCopyContext *context) {
  PycAST *a = new PycAST(*this);
  if (context)
    for (int i = 0; i < a->pnodes.n; i++) a->pnodes[i] = context->nmap->get(a->pnodes.v[i]);
  return a;
}

IFAAST *PycAST::copy_tree(ASTCopyContext *context) {
  PycAST *a = (PycAST *)copy_node(context);
  for (int i = 0; i < a->children.n; i++) a->children[i] = (PycAST *)a->children.v[i]->copy_tree(context);
  return a;
}

Vec<Fun *> *PycAST::visible_functions(Sym *arg0) {
  Vec<Fun *> *v = 0;
  if (arg0->fun) {
    Fun *f = arg0->fun;
    v = new Vec<Fun *>;
    v->add(f);
    return v;
  }
  return NULL;
}

static cchar *stmt_string(enum _stmt_kind k) {
  switch (k) {
    default:
      assert(!"bad case");
    case FunctionDef_kind:
      return "FunctionDef";
    case ClassDef_kind:
      return "ClassDef";
    case Return_kind:
      return "Return";
    case Delete_kind:
      return "Delete";
    case Assign_kind:
      return "Assign";
    case AugAssign_kind:
      return "AugAssign";
    case Print_kind:
      return "Print";
    case For_kind:
      return "For";
    case While_kind:
      return "While";
    case If_kind:
      return "If";
    case With_kind:
      return "With";
    case Raise_kind:
      return "Raise";
    case TryExcept_kind:
      return "TryExcept";
    case TryFinally_kind:
      return "TryFinally";
    case Assert_kind:
      return "Assert";
    case Import_kind:
      return "Import";
    case ImportFrom_kind:
      return "ImportFrom";
    case Exec_kind:
      return "Exec";
    case Global_kind:
      return "Global";
#if PY_MAJOR_VERSION == 3
    case NonLocal_kind:
      return "Global";
#endif
    case Expr_kind:
      return "Expr";
    case Pass_kind:
      return "Pass";
    case Break_kind:
      return "Break";
    case Continue_kind:
      return "Continue";
  }
};

static cchar *expr_string(enum _expr_kind k) {
  switch (k) {
    default:
      assert(!"bad case");
    case BoolOp_kind:
      return "BoolOp";
    case BinOp_kind:
      return "BinOp";
    case UnaryOp_kind:
      return "UnaryOp";
    case Lambda_kind:
      return "Lambda";
    case IfExp_kind:
      return "IfExp";
    case Dict_kind:
      return "Dict";
    case ListComp_kind:
      return "ListComp";
#if PY_MAJOR_VERSION == 3
    case SetComp_kind:
      return "SetComp";
#endif
    case GeneratorExp_kind:
      return "GeneratorExp";
    case Yield_kind:
      return "Yield";
    case Compare_kind:
      return "Compare";
    case Call_kind:
      return "Call";
    case Repr_kind:
      return "Repr";
    case Num_kind:
      return "Num";
    case Str_kind:
      return "Str";
    case Attribute_kind:
      return "Attribute";
    case Subscript_kind:
      return "Subscript";
#if PY_MAJOR_VERSION == 3
    case Starred_kind:
      return "Starred";
#endif
    case Name_kind:
      return "Name";
    case List_kind:
      return "List";
    case Tuple_kind:
      return "Tuple";
  }
}

static void ast_html(PycAST *a, FILE *fp, Fun *f, int indent) {
  Sym *s = a->sym && a->sym->name ? a->sym : a->rval;
  if (a->xstmt) {
    fprintf(fp, "<li>%s %s\n", stmt_string(a->xstmt->kind), s && s->name ? s->name : "");
  } else if (a->xexpr) {
    fprintf(fp, "<li>%s %s %s\n", expr_string(a->xexpr->kind), s && s->name ? s->name : "",
            s->is_constant && s->constant ? s->constant : "");
    if ((!s->is_constant || !s->constant) && a->rval) {
      Vec<Sym *> consts;
      if (constant_info(a, consts, a->rval)) {
        fprintf(fp, ":constants {");
        for (auto s : consts.values()) {
          fprintf(fp, " ");
          fprint_imm(fp, s->imm);
        }
        fprintf(fp, " }\n");
      }
    }
  }
  if (a->pre_scope_children.n + a->children.n > 0) fprintf(fp, "<ul>\n");
  for (int i = 0; i < a->pre_scope_children.n; i++) ast_html(a->pre_scope_children[i], fp, f, indent + 1);
  for (int i = 0; i < a->children.n; i++) ast_html(a->children[i], fp, f, indent + 1);
  if (a->pre_scope_children.n + a->children.n > 0) fprintf(fp, "</ul>\n");
  fprintf(fp, "</li>\n");
}

void PycAST::html(FILE *fp, Fun *f) { ast_html(this, fp, f, 0); }
