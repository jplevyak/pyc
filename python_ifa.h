#ifndef _python_ifa_H_
#define _python_ifa_H_

#include "defs.h"

class BaseIFAAST;
class Label;
class Code;
class IFAASTCopyContext;
class Sym;

typedef PySTEntryObject Symbol;

class PycCallbacks : public IFACallbacks {
public:
  void finalize_functions();
  Sym *new_Sym(cchar *name = 0);
};

class PycSymbol : public IFASymbol {
 public:
  Sym *clone();
  cchar *pathname();
  int line();
  int source_line();
  int ast_id();
  PycSymbol *copy();

  Symbol *symbol;
  cchar *filename;
  
  PycSymbol();
};

class PycAST : public IFAAST {
 public:
  cchar *pathname();
  int source_line();
  int line();
  Sym *symbol();  
  IFAAST *copy_tree(ASTCopyContext* context);
  IFAAST *copy_node(ASTCopyContext* context);
  Vec<Fun *> *visible_functions(Sym *arg0);

  stmt_ty xstmt;
  expr_ty xexpr;
  cchar *filename;
  PycAST *parent;
  Vec<PycAST *> pre_scope_children;
  Vec<PycAST *> children;

  Code *code;           // IF1 Code (including children)
  Label *label[2];      // before and after for loops (continue,break)
  Sym *sym, *rval;      // IF1 Syms

  uint32 is_builtin:1;
  uint32 is_member:1;
  
  bool is_call() { return xexpr && xexpr->kind == Call_kind; }
  bool is_assign() { return xstmt && (xstmt->kind == Assign_kind || xstmt->kind == AugAssign_kind); }

  PycAST();
};

class PycModule : public gc { public:
  mod_ty mod;
  cchar *filename;
  int is_builtin;
  PycModule(mod_ty amod, cchar *afilename, int an_is_builtin)
    : mod(amod), filename(afilename), is_builtin(an_is_builtin) {}
};

int ast_to_if1(Vec<PycModule *> &mods);

#endif
