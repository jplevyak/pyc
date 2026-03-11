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
Map<PyDAST *, PycAST *> pydmap;
Sym *sym_long = 0, *sym_ellipsis = 0, *sym_ellipsis_type = 0, *sym_unicode = 0, *sym_buffer = 0, *sym_xrange = 0,
    *sym_declare = 0;

#define S(_x) Sym *sym_##_x = 0;
#include "pyc_symbols.h"

cchar *cannonical_self = 0;
int finalized_aspect = 0;
Vec<Sym *> builtin_functions;

PycCallbacks::~PycCallbacks() {}

PycSymbol::PycSymbol() : filename(0), previous(0) {}

void PycCompiler::init() {
  lineno = -1;
  node = 0;
  mod = package = 0;
  modules = 0;
  search_path = 0;
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
  if (a->xpyd) return a->xpyd->line;
  return 0;
}

int PycSymbol::line() {
  PycAST *a = (PycAST *)sym->ast;
  if (!a) return 0;
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
    : xpyd(0),
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

int PycAST::column() { return 0; }

int PycAST::line() {
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

static void ast_html(PycAST *a, FILE *fp, Fun *f, int indent) {
  Sym *s = a->sym && a->sym->name ? a->sym : a->rval;
  if (a->xpyd) {
    fprintf(fp, "<li>node %s\n", s && s->name ? s->name : "");
  }
  if (a->children.n > 0) fprintf(fp, "<ul>\n");
  for (int i = 0; i < a->children.n; i++) ast_html(a->children[i], fp, f, indent + 1);
  if (a->children.n > 0) fprintf(fp, "</ul>\n");
  fprintf(fp, "</li>\n");
}

void PycAST::html(FILE *fp, Fun *f) { ast_html(this, fp, f, 0); }
