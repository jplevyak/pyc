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
  // Python's bool is a genuine int subtype (isinstance(True, int) is
  // True) -- opt into ifa's numeric lattice for bool (ifa.h, ifa/issues/081)
  // so int/bool arithmetic folds and types correctly instead of either
  // crashing (081) or silently salvaging to an untyped expression.
  bool bool_is_numeric() { return true; }
  // ifa/issues/082: the exact wrapper-call names pyc's own frontend
  // lowers `isinstance(...)`/`x is None`/`x is not None` to (see
  // python_ifa_build_if1.cc and __pyc__/05_builtins.py) -- lets ifa's
  // core FA narrowing (analysis/fa.cc) recognize them without ifa
  // itself hardcoding Python's names.
  cchar *narrowing_isinstance_name() { return "isinstance"; }
  cchar *narrowing_is_none_name() { return "__is__"; }
  cchar *narrowing_is_not_none_name() { return "__nis__"; }
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
  // Set alongside is_object_index when the index trailer is a slice
  // (`a[i:j]`) rather than a plain subscript (`a[i]`) -- slice STORE
  // targets still eagerly build the __pyc_setslice__ call (rval is
  // its result, consumed via find_send()+add_arg by assign/augassign
  // callers); plain-index STORE targets defer instead (rval/sym hold
  // the object/index pair so augmented assignment can __getitem__
  // before __setitem__ -- see PY_augassign's is_object_index branch).
  uint32 is_slice : 1;
  // @staticmethod / @classmethod markers, set on a class-body
  // funcdef's PycAST during build_syms_pyda's PY_decorated case and
  // consumed by gen_fun_pyda (formal-list convention) and the
  // decorator-application loop (markers are not real decorators).
  uint32 is_staticmethod : 1;
  uint32 is_classmethod : 1;

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
  // issues/113: loaded as <dir>/__init__.py. PEP 328 resolves a relative
  // import against the module's PACKAGE, which for a package is itself
  // and for a plain module is its parent.
  bool is_package = false;
  PycModule(cchar *afilename, bool ais_builtin = false)
      : pymod(nullptr), filename(afilename), name_sym(0), file_sym(0), ctx(0), is_builtin(ais_builtin), built_if1(false) {
    name = mod_name_from_filename(filename);
  }
};

int ast_to_if1(Vec<PycModule *> &mods);

// issue 011 (per-callee can-raise gating, post-FA refinement): call
// once after ifa_analyze() succeeds (pyc.cc), before ifa_optimize()
// or codegen -- that's the earliest point Fun::calls (built by
// clone(), partway through ifa_analyze()) exists and is stable.
void compute_fun_can_raise();

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
// issue 069: generate tuple __eq__/__lt__ at the program's max tuple arity
// (min_arity floors it -- the REPL passes a generous value since it can't
// pre-scan future interactive input) and append them to the builtin tuple.
void inject_tuple_methods(Vec<PycModule *> &mods, int min_arity);

#endif
