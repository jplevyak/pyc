#ifndef _python_ifa_H_
#define _python_ifa_H_

#include "defs.h"
#include "pattern.h"

class BaseIFAAST;
class Label;
class Code;
class IFAASTCopyContext;
class Sym;
struct PycContext;

typedef PySTEntryObject Symbol;

class PycCallbacks : public IFACallbacks {
 public:
  PycContext *ctx;
  void finalize_functions();
  Sym *new_Sym(cchar *name = 0);
  Fun *default_wrapper(Fun *, Vec<MPosition *> &defaults);
  bool reanalyze(Vec<ATypeViolation *> &type_violations);
  bool c_codegen_pre_file(FILE *);
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

  Symbol *symbol;
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

  stmt_ty xstmt;
  expr_ty xexpr;
  cchar *filename;
  PycAST *parent;
  Vec<PycAST *> pre_scope_children;
  Vec<PycAST *> children;

  Code *code;       // IF1 Code (including children)
  Label *label[2];  // before and after for loops (continue,break)
  Sym *sym, *rval;  // IF1 Syms

  uint32 is_builtin : 1;
  uint32 is_member : 1;
  uint32 is_object_index : 1;

  bool is_call() { return xexpr && xexpr->kind == Call_kind; }
  bool is_assign() { return xstmt && (xstmt->kind == Assign_kind || xstmt->kind == AugAssign_kind); }

  PycAST();
};

cchar *mod_name_from_filename(cchar *);

class PycModule : public gc {
 public:
  mod_ty mod;
  cchar *filename;
  cchar *name;
  PycSymbol *name_sym;
  PycSymbol *file_sym;
  PycContext *ctx;
  bool is_builtin;
  bool built_if1;
  PycModule(mod_ty amod, cchar *afilename, bool ais_builtin = false)
      : mod(amod), filename(afilename), name_sym(0), file_sym(0), ctx(0), is_builtin(ais_builtin), built_if1(false) {
    name = mod_name_from_filename(filename);
  }
};

mod_ty file_to_mod(cchar *filename, PyArena *arena);
int ast_to_if1(Vec<PycModule *> &mods, PyArena *arena);

#endif
