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
  // issues/001: for a PY_lambda/PY_funcdef node, the closure-carrier
  // class synthesized during build_syms_pyda if this scope captures
  // any enclosing-function locals (null otherwise -- the common,
  // unaffected case). Set once in build_syms_pyda, read back during
  // build_if1_pyda's construction of the closure-creation-site code.
  Sym *closure_cls;

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

// Stage-3 REPL: split ast_to_if1 into a one-time baseline (builtin module
// only) and a per-iteration extend (user module).  The REPL parent calls
// ast_to_if1_baseline once; each fork child inherits the IF1 state via CoW
// and calls ast_to_if1_extend instead of ast_to_if1.
struct BaselineIF1State {
  PycCompiler *ctx;   // GC-allocated; persists across fork children
  Code *code;         // code chain tail after processing the builtin module
};

// One-time setup: initialise if1/pdb/ctx and build IF1 for the builtin module.
// builtin_mods must remain live for the process lifetime (use a static Vec).
BaselineIF1State ast_to_if1_baseline(Vec<PycModule *> &builtin_mods);

// Per-REPL-iteration (called in fork child): extend the inherited IF1 state
// with the user module(s) in all_mods[1..] and finalise the program.
// all_mods[0] must be the same builtin module passed to ast_to_if1_baseline.
int ast_to_if1_extend(Vec<PycModule *> &all_mods, BaselineIF1State bl);

#endif
