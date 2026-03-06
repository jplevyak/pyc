#pragma once
/*
  Copyright 2008-2025 John Plevyak, All Rights Reserved
  Python DParser AST node definitions.
*/
#include "common.h"

// Forward declarations for IFA annotations (Phase 3+)
class Code;
class Label;
class Sym;

enum PyASTKind {
  PY_invalid = 0,
  // Module
  PY_module,
  // Definitions
  PY_funcdef, PY_classdef, PY_decorated, PY_decorator,
  // Block
  PY_suite,
  // Statements
  PY_expr_stmt, PY_assign, PY_augassign,
  PY_print_stmt, PY_del_stmt, PY_pass_stmt,
  PY_return_stmt, PY_break_stmt, PY_continue_stmt,
  PY_raise_stmt, PY_yield_stmt,
  PY_import_name, PY_import_from,
  PY_import_as_name, PY_dotted_as_name, PY_dotted_name,
  PY_global_stmt, PY_exec_stmt, PY_assert_stmt,
  // Compound statement parts
  PY_if_stmt, PY_elif_clause, PY_else_clause,
  PY_while_stmt, PY_for_stmt,
  PY_try_stmt, PY_except_clause, PY_except_handler, PY_finally_clause,
  PY_with_stmt, PY_with_item,
  // Expressions
  PY_ternary,
  PY_bool_or, PY_bool_and, PY_bool_not,
  PY_compare, PY_cmp_op,
  PY_binop, PY_unaryop,
  PY_call, PY_attribute, PY_subscript, PY_power,
  PY_lambda, PY_yield_expr,
  // Atoms
  PY_name, PY_number, PY_string, PY_backquote,
  PY_tuple, PY_list, PY_dict, PY_set,
  PY_listcomp, PY_genexpr,
  PY_slice,
  // Function arguments/parameters
  PY_parameters, PY_varargslist, PY_fpdef, PY_fplist,
  PY_arglist, PY_keyword_arg, PY_star_arg, PY_dstar_arg, PY_arg_default,
  // Collections / iteration
  PY_testlist, PY_exprlist, PY_testlist1,
  PY_comp_for, PY_comp_if,
  PY_list_for, PY_list_if,
  PY_subscriptlist,
  PY_MAX
};

enum PyOp {
  PY_OP_NONE = 0,
  PY_OP_ADD, PY_OP_SUB, PY_OP_MUL, PY_OP_DIV, PY_OP_MOD,
  PY_OP_POW, PY_OP_LSHIFT, PY_OP_RSHIFT,
  PY_OP_BITOR, PY_OP_BITXOR, PY_OP_BITAND, PY_OP_FLOORDIV,
  PY_OP_UADD, PY_OP_USUB, PY_OP_INVERT,
  PY_CMP_EQ, PY_CMP_NE, PY_CMP_LT, PY_CMP_LE,
  PY_CMP_GT, PY_CMP_GE,
  PY_CMP_IS, PY_CMP_IS_NOT, PY_CMP_IN, PY_CMP_NOT_IN,
  PY_CMP_LTGT,
  PY_OP_MAX
};

enum PyCtx { PY_LOAD = 0, PY_STORE, PY_DEL };

class PyDAST : public gc {
 public:
  PyASTKind kind;
  Vec<PyDAST *> children;
  cchar *str_val;   // for PY_name, PY_string, PY_number (raw text), PY_dotted_name
  long int_val;     // for integer literals
  double float_val; // for float literals
  bool is_int;      // true if int_val is valid
  bool is_imag;     // imaginary number suffix
  int op;           // PyOp
  int ctx;          // PyCtx
  cchar *filename;
  int line;
  // IFA annotations (Phase 3+)
  Code *code;
  Label *label[2];
  Sym *sym;
  Sym *rval;
  PyDAST *parent;
  unsigned is_builtin : 1;
  unsigned is_member : 1;
  unsigned is_object_index : 1;

  PyDAST()
      : kind(PY_invalid), str_val(nullptr), int_val(0), float_val(0.0),
        is_int(false), is_imag(false), op(0), ctx(0),
        filename(nullptr), line(0),
        code(nullptr), sym(nullptr), rval(nullptr),
        parent(nullptr), is_builtin(0), is_member(0), is_object_index(0) {
    label[0] = label[1] = nullptr;
  }

  void add(PyDAST *c) {
    if (c) {
      c->parent = this;
      children.add(c);
    }
  }
};
