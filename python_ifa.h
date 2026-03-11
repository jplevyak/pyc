#ifndef _python_ifa_H_
#define _python_ifa_H_

#include "defs.h"
#include "pattern.h"

class BaseIFAAST;
class Label;
class Code;
class IFAASTCopyContext;
class Sym;
class PycCompiler;
class PyDAST;

class PycCallbacks : public IFACallbacks {
 public:
  virtual ~PycCallbacks();
};

class PycSymbol : public IFASymbol {
 public:
  Sym *clone();
  cchar *pathname();
  int column();
  int line();
  int source_line();
  int ast_id();
  PycSymbol *copy();

  cchar *filename;
  PycSymbol *previous;

  PycSymbol();
};

class PycAST : public IFAAST {
 public:
  cchar *pathname();
  int column();
  int line();
  int source_line();
  Sym *symbol();
  void html(FILE *fp, Fun *f);
  IFAAST *copy_tree(ASTCopyContext *context);
  IFAAST *copy_node(ASTCopyContext *context);
  Vec<Fun *> *visible_functions(Sym *arg0);

  PyDAST *xpyd;  // DParser AST node
  cchar *filename;
  PycAST *parent;
  Vec<PycAST *> children;

  Code *code;       // IF1 Code (including children)
  Label *label[2];  // before and after for loops (continue,break)
  Sym *sym, *rval;  // IF1 Syms

  uint32 is_builtin : 1;
  uint32 is_member : 1;
  uint32 is_object_index : 1;

  PycAST();
};

cchar *mod_name_from_filename(cchar *);

class PycModule : public gc {
 public:
  PyDAST *pymod;
  cchar *filename;
  cchar *name;
  PycSymbol *name_sym;
  PycSymbol *file_sym;
  PycCompiler *ctx;
  bool is_builtin;
  bool built_if1;
  PycModule(cchar *afilename, bool ais_builtin = false)
      : pymod(nullptr), filename(afilename), name_sym(0), file_sym(0), ctx(0), is_builtin(ais_builtin), built_if1(false) {
    name = mod_name_from_filename(filename);
  }
};

int ast_to_if1(Vec<PycModule *> &mods);

#endif
