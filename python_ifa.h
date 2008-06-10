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
  Sym *new_Sym(char *name = 0);
};

class PycSymbol : public IFASymbol {
 public:
  Sym *clone();
  char *pathname();
  int line();
  int source_line();
  int ast_id();
  PycSymbol *copy();

  Symbol *symbol;

  PycSymbol();
};

class PycAST : public IFAAST {
 public:
  char *pathname();
  int source_line();
  int line();
  Sym *symbol();  
  IFAAST *copy_tree(ASTCopyContext* context);
  IFAAST *copy_node(ASTCopyContext* context);
  Vec<Fun *> *visible_functions(Sym *arg0);

  stmt_ty xstmt;
  expr_ty xexpr;
  Vec<PycAST *> pre_scope_children;
  Vec<PycAST *> children;

  Code *code;           // IF1 Code (including children)
  Label *label[2];      // before and after for loops (continue,break)
  Sym *sym, *rval;      // IF1 Syms


  PycAST();
};

#endif
