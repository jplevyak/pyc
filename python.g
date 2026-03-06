/* Grammar from Python 2.7 Grammar/Grammar, converted to dparser */

{
#include "python_parse.h"
#undef D_ParseNode_Globals
#define D_ParseNode_Globals PythonGlobals

#include "ifadefs.h"
#include "ast_to_if1.h"
#include "make_ast.h"
#include "dparse.h"
#include "dparse_tables.h"

extern D_Symbol d_symbols_python[];

  int python_indent(PythonGlobals **p_globals);
  int python_dedent(PythonGlobals **p_globals);
  void python_whitespace(struct D_Parser *p, d_loc_t *loc, void **p_globals);
  int dparser_python_user_size = sizeof(D_ParseNode_User);
  int dparser_python_globals_size = sizeof(D_ParseNode_Globals);
}

${declare longest_match}
${declare subparser single_input}
${declare subparser eval_input}
${declare whitespace python_whitespace}

file_input: (NL | stmt)* {
  $$.ast = new_pyast(PY_module, &$n);
  dig_collect($$.ast, &$n);
  ((PythonGlobals *)$g)->root_ast = $$.ast;
};

single_input: NL | simple_stmt | compound_stmt NL;
eval_input: testlist NL*;

decorator: '@' dotted_name ( LP arglist? RP )? NL {
  $$.ast = new_pyast(PY_decorator, &$n);
  dig_collect($$.ast, &$n);
};
decorators: decorator+ {
  $$.ast = new_pyast(PY_suite, &$n);
  dig_collect($$.ast, &$n);
};
decorated: decorators (classdef | funcdef) {
  $$.ast = new_pyast(PY_decorated, &$n);
  dig_collect($$.ast, &$n);
};
funcdef: 'def' NAME parameters ':' suite {
  $$.ast = new_pyast(PY_funcdef, &$n);
  dig_collect($$.ast, &$n);
};
parameters: LP varargslist? RP {
  $$.ast = new_pyast(PY_parameters, &$n);
  dig_collect($$.ast, &$n);
};
varargsarg: fpdef '=' test { $$.ast = new_pyast(PY_arg_default, &$n); $$.ast->add($0.ast); $$.ast->add($2.ast); }
           | fpdef { $$.ast = $0.ast; };
star_arg: '*' NAME { $$.ast = new_pyast(PY_star_arg, &$n); $$.ast->add($1.ast); };
dstar_arg: '**' NAME { $$.ast = new_pyast(PY_dstar_arg, &$n); $$.ast->add($1.ast); };
varargslist: varargsarg (',' varargsarg)* (',' (star_arg (',' dstar_arg)? | dstar_arg))? ','? {
  $$.ast = new_pyast(PY_varargslist, &$n);
  dig_collect($$.ast, &$n);
}
| star_arg (',' dstar_arg)? ','? {
  $$.ast = new_pyast(PY_varargslist, &$n);
  dig_collect($$.ast, &$n);
}
| dstar_arg ','? {
  $$.ast = new_pyast(PY_varargslist, &$n);
  dig_collect($$.ast, &$n);
};
fpdef: NAME | LP fplist RP {
  $$.ast = new_pyast(PY_fpdef, &$n);
  dig_collect($$.ast, &$n);
};
fplist: fpdef (',' fpdef)* ','? {
  $$.ast = new_pyast(PY_fplist, &$n);
  dig_collect($$.ast, &$n);
};

stmt: simple_stmt | compound_stmt;
simple_stmt: small_stmt (';' small_stmt)* ';'? NL {
  /* If only one small_stmt (ebnf_nc == 0), pass through; else wrap in suite */
  if (d_get_number_of_children(d_get_child(&$n, 1)) == 0) {
    $$.ast = $0.ast;
  } else {
    $$.ast = new_pyast(PY_suite, &$n);
    dig_collect($$.ast, &$n);
  }
};
small_stmt: expr_stmt | print_stmt | del_stmt | pass_stmt | flow_stmt | import_stmt | global_stmt | exec_stmt | assert_stmt;

expr_stmt: testlist (augassign testlist | ('=' testlist)*) {
  $$.ast = new_pyast(PY_expr_stmt, &$n);
  dig_collect($$.ast, &$n);
  if ($$.ast->children.n >= 2 && $$.ast->children.v[1]->kind == PY_augassign)
    $$.ast->kind = PY_augassign;
  else if ($$.ast->children.n >= 2)
    $$.ast->kind = PY_assign;
};
augassign: '+=' { $$.ast = new_pyast(PY_augassign, &$n); $$.ast->op = PY_OP_ADD; }
         | '-=' { $$.ast = new_pyast(PY_augassign, &$n); $$.ast->op = PY_OP_SUB; }
         | '*=' { $$.ast = new_pyast(PY_augassign, &$n); $$.ast->op = PY_OP_MUL; }
         | '/=' { $$.ast = new_pyast(PY_augassign, &$n); $$.ast->op = PY_OP_DIV; }
         | '%=' { $$.ast = new_pyast(PY_augassign, &$n); $$.ast->op = PY_OP_MOD; }
         | '&=' { $$.ast = new_pyast(PY_augassign, &$n); $$.ast->op = PY_OP_BITAND; }
         | '|=' { $$.ast = new_pyast(PY_augassign, &$n); $$.ast->op = PY_OP_BITOR; }
         | '^=' { $$.ast = new_pyast(PY_augassign, &$n); $$.ast->op = PY_OP_BITXOR; }
         | '<<=' { $$.ast = new_pyast(PY_augassign, &$n); $$.ast->op = PY_OP_LSHIFT; }
         | '>>=' { $$.ast = new_pyast(PY_augassign, &$n); $$.ast->op = PY_OP_RSHIFT; }
         | '**=' { $$.ast = new_pyast(PY_augassign, &$n); $$.ast->op = PY_OP_POW; }
         | '//=' { $$.ast = new_pyast(PY_augassign, &$n); $$.ast->op = PY_OP_FLOORDIV; }
         ;
print_stmt: 'print' ( ( test (',' test)* ','? )? | '>>' test ( (',' test)+ ','? )? ) {
  $$.ast = new_pyast(PY_print_stmt, &$n);
  dig_collect($$.ast, &$n);
};
del_stmt: 'del' exprlist {
  $$.ast = new_pyast(PY_del_stmt, &$n);
  dig_collect($$.ast, &$n);
};
pass_stmt: 'pass' {
  $$.ast = new_pyast(PY_pass_stmt, &$n);
};
flow_stmt: break_stmt | continue_stmt | return_stmt | raise_stmt | yield_stmt;
break_stmt: 'break' {
  $$.ast = new_pyast(PY_break_stmt, &$n);
};
continue_stmt: 'continue' {
  $$.ast = new_pyast(PY_continue_stmt, &$n);
};
return_stmt: 'return' testlist? {
  $$.ast = new_pyast(PY_return_stmt, &$n);
  dig_collect($$.ast, &$n);
};
raise_stmt: 'raise' (test (',' test (',' test)?)?)? {
  $$.ast = new_pyast(PY_raise_stmt, &$n);
  dig_collect($$.ast, &$n);
};
yield_stmt: 'yield' testlist {
  $$.ast = new_pyast(PY_yield_stmt, &$n);
  dig_collect($$.ast, &$n);
};
import_stmt: import_name | import_from;
import_name: 'import' dotted_as_names {
  $$.ast = new_pyast(PY_import_name, &$n);
  dig_collect($$.ast, &$n);
};
import_from: ('from' ('.'* dotted_name | '.'+)
              'import' ('*' | '(' import_as_names ')' | import_as_names)) {
  $$.ast = new_pyast(PY_import_from, &$n);
  dig_collect($$.ast, &$n);
};
import_as_name: NAME ('as' NAME)? {
  $$.ast = new_pyast(PY_import_as_name, &$n);
  dig_collect($$.ast, &$n);
};
dotted_as_name: dotted_name ('as' NAME)? {
  $$.ast = new_pyast(PY_dotted_as_name, &$n);
  dig_collect($$.ast, &$n);
};
import_as_names: import_as_name (',' import_as_name)* ','? {
  if (d_get_number_of_children(d_get_child(&$n, 1)) == 0) {
    $$.ast = $0.ast;
  } else {
    $$.ast = new_pyast(PY_testlist, &$n);
    dig_collect($$.ast, &$n);
  }
};
dotted_as_names: dotted_as_name (',' dotted_as_name)* {
  if (d_get_number_of_children(d_get_child(&$n, 1)) == 0) {
    $$.ast = $0.ast;
  } else {
    $$.ast = new_pyast(PY_testlist, &$n);
    dig_collect($$.ast, &$n);
  }
};
dotted_name: NAME ('.' NAME)* {
  $$.ast = new_pyast(PY_dotted_name, &$n);
  int len = (int)($n.end - $n.start_loc.s);
  $$.ast->str_val = pyast_dupstr($n.start_loc.s, len);
};
global_stmt: 'global' NAME (',' NAME)* {
  $$.ast = new_pyast(PY_global_stmt, &$n);
  dig_collect($$.ast, &$n);
};
exec_stmt: 'exec' expr ('in' test (',' test)?)? {
  $$.ast = new_pyast(PY_exec_stmt, &$n);
  dig_collect($$.ast, &$n);
};
assert_stmt: 'assert' test (',' test)? {
  $$.ast = new_pyast(PY_assert_stmt, &$n);
  dig_collect($$.ast, &$n);
};

compound_stmt: if_stmt | while_stmt | for_stmt | try_stmt | with_stmt | funcdef | classdef | decorated;

/* Named clause sub-rules for cleaner AST */
elif_clause: 'elif' test ':' suite {
  $$.ast = new_pyast(PY_elif_clause, &$n);
  dig_collect($$.ast, &$n);
};
else_clause: 'else' ':' suite {
  $$.ast = new_pyast(PY_else_clause, &$n);
  dig_collect($$.ast, &$n);
};
except_handler: except_clause ':' suite {
  $$.ast = new_pyast(PY_except_handler, &$n);
  dig_collect($$.ast, &$n);
};
finally_clause: 'finally' ':' suite {
  $$.ast = new_pyast(PY_finally_clause, &$n);
  dig_collect($$.ast, &$n);
};

if_stmt: 'if' test ':' suite elif_clause* else_clause? {
  $$.ast = new_pyast(PY_if_stmt, &$n);
  dig_collect($$.ast, &$n);
};
while_stmt: 'while' test ':' suite else_clause? {
  $$.ast = new_pyast(PY_while_stmt, &$n);
  dig_collect($$.ast, &$n);
};
for_stmt: 'for' exprlist 'in' testlist ':' suite else_clause? {
  $$.ast = new_pyast(PY_for_stmt, &$n);
  dig_collect($$.ast, &$n);
};
try_stmt: ('try' ':' suite
           ((except_handler)+
            else_clause?
            finally_clause? |
            finally_clause)) {
  $$.ast = new_pyast(PY_try_stmt, &$n);
  dig_collect($$.ast, &$n);
};
with_stmt: 'with' with_item (',' with_item)*  ':' suite {
  $$.ast = new_pyast(PY_with_stmt, &$n);
  dig_collect($$.ast, &$n);
};
with_item: test ('as' expr)? {
  $$.ast = new_pyast(PY_with_item, &$n);
  dig_collect($$.ast, &$n);
};
except_clause: 'except' (test (',' test)?)? {
  $$.ast = new_pyast(PY_except_clause, &$n);
  dig_collect($$.ast, &$n);
};
suite: simple_stmt
     | NL INDENT stmt+ DEDENT {
         $$.ast = new_pyast(PY_suite, &$n);
         dig_collect($$.ast, &$n);
       };

testlist_safe: old_test ((',' old_test)+ ','?)? {
  if (d_get_number_of_children(d_get_child(&$n, 1)) == 0) {
    $$.ast = $0.ast;
  } else {
    $$.ast = new_pyast(PY_testlist, &$n);
    dig_collect($$.ast, &$n);
  }
};
old_test: or_test | old_lambdef;
old_lambdef: 'lambda' varargslist? ':' old_test {
  $$.ast = new_pyast(PY_lambda, &$n);
  dig_collect($$.ast, &$n);
};

test: or_test ('if' or_test 'else' test)? {
    /* $# == 2 when ternary matches, $# == 1 for plain or_test */
    if (d_get_number_of_children(d_get_child(&$n, 1)) == 0) {
      $$.ast = $0.ast;
    } else {
      $$.ast = new_pyast(PY_ternary, &$n);
      dig_collect($$.ast, &$n);
    }
  }
  | lambdef;
or_test: and_test ('or' and_test)* {
  if (d_get_number_of_children(d_get_child(&$n, 1)) == 0) {
    $$.ast = $0.ast;
  } else {
    $$.ast = new_pyast(PY_bool_or, &$n);
    dig_collect($$.ast, &$n);
  }
};
and_test: not_test ('and' not_test)* {
  if (d_get_number_of_children(d_get_child(&$n, 1)) == 0) {
    $$.ast = $0.ast;
  } else {
    $$.ast = new_pyast(PY_bool_and, &$n);
    dig_collect($$.ast, &$n);
  }
};
not_test: 'not' not_test {
    $$.ast = new_pyast(PY_bool_not, &$n);
    $$.ast->add($1.ast);
  }
  | comparison;
comparison: expr (comp_op expr)* {
  if (d_get_number_of_children(d_get_child(&$n, 1)) == 0) {
    $$.ast = $0.ast;
  } else {
    $$.ast = new_pyast(PY_compare, &$n);
    dig_collect($$.ast, &$n);
  }
};
comp_op: '<'      { $$.ast = new_pyast(PY_cmp_op, &$n); $$.ast->op = PY_CMP_LT; }
       | '>'      { $$.ast = new_pyast(PY_cmp_op, &$n); $$.ast->op = PY_CMP_GT; }
       | '=='     { $$.ast = new_pyast(PY_cmp_op, &$n); $$.ast->op = PY_CMP_EQ; }
       | '>='     { $$.ast = new_pyast(PY_cmp_op, &$n); $$.ast->op = PY_CMP_GE; }
       | '<='     { $$.ast = new_pyast(PY_cmp_op, &$n); $$.ast->op = PY_CMP_LE; }
       | '<>'     { $$.ast = new_pyast(PY_cmp_op, &$n); $$.ast->op = PY_CMP_LTGT; }
       | '!='     { $$.ast = new_pyast(PY_cmp_op, &$n); $$.ast->op = PY_CMP_NE; }
       | 'in'     { $$.ast = new_pyast(PY_cmp_op, &$n); $$.ast->op = PY_CMP_IN; }
       | 'not' 'in' { $$.ast = new_pyast(PY_cmp_op, &$n); $$.ast->op = PY_CMP_NOT_IN; }
       | 'is'     { $$.ast = new_pyast(PY_cmp_op, &$n); $$.ast->op = PY_CMP_IS; }
       | 'is' 'not' { $$.ast = new_pyast(PY_cmp_op, &$n); $$.ast->op = PY_CMP_IS_NOT; }
       ;
expr: xor_expr ('|' xor_expr)* {
  D_ParseNode *list = d_get_child(&$n, 1);
  int nc = d_get_number_of_children(list);
  if (nc == 0) { $$.ast = $0.ast; }
  else {
    PyDAST *result = $0.ast;
    for (int i = 0; i < nc; i++) {
      D_ParseNode *iter = d_get_child(list, i);
      PyDAST *right = dig_pyast(iter);
      PyDAST *b = new_pyast(PY_binop, iter);
      b->op = PY_OP_BITOR;
      b->add(result); b->add(right);
      result = b;
    }
    $$.ast = result;
  }
};
xor_expr: and_expr ('^' and_expr)* {
  D_ParseNode *list = d_get_child(&$n, 1);
  int nc = d_get_number_of_children(list);
  if (nc == 0) { $$.ast = $0.ast; }
  else {
    PyDAST *result = $0.ast;
    for (int i = 0; i < nc; i++) {
      D_ParseNode *iter = d_get_child(list, i);
      PyDAST *right = dig_pyast(iter);
      PyDAST *b = new_pyast(PY_binop, iter);
      b->op = PY_OP_BITXOR;
      b->add(result); b->add(right);
      result = b;
    }
    $$.ast = result;
  }
};
and_expr: shift_expr ('&' shift_expr)* {
  D_ParseNode *list = d_get_child(&$n, 1);
  int nc = d_get_number_of_children(list);
  if (nc == 0) { $$.ast = $0.ast; }
  else {
    PyDAST *result = $0.ast;
    for (int i = 0; i < nc; i++) {
      D_ParseNode *iter = d_get_child(list, i);
      PyDAST *right = dig_pyast(iter);
      PyDAST *b = new_pyast(PY_binop, iter);
      b->op = PY_OP_BITAND;
      b->add(result); b->add(right);
      result = b;
    }
    $$.ast = result;
  }
};
shift_expr: arith_expr (('<<'|'>>') arith_expr)* {
  D_ParseNode *list = d_get_child(&$n, 1);
  int nc = d_get_number_of_children(list);
  if (nc == 0) { $$.ast = $0.ast; }
  else {
    PyDAST *result = $0.ast;
    for (int i = 0; i < nc; i++) {
      D_ParseNode *iter = d_get_child(list, i);
      char opch = iter->start_loc.s[0];
      PyDAST *right = dig_pyast(iter);
      PyDAST *b = new_pyast(PY_binop, iter);
      b->op = (opch == '<') ? PY_OP_LSHIFT : PY_OP_RSHIFT;
      b->add(result); b->add(right);
      result = b;
    }
    $$.ast = result;
  }
};
arith_expr: term (('+'|'-') term)* {
  D_ParseNode *list = d_get_child(&$n, 1);
  int nc = d_get_number_of_children(list);
  if (nc == 0) { $$.ast = $0.ast; }
  else {
    PyDAST *result = $0.ast;
    for (int i = 0; i < nc; i++) {
      D_ParseNode *iter = d_get_child(list, i);
      char opch = iter->start_loc.s[0];
      PyDAST *right = dig_pyast(iter);
      PyDAST *b = new_pyast(PY_binop, iter);
      b->op = (opch == '+') ? PY_OP_ADD : PY_OP_SUB;
      b->add(result); b->add(right);
      result = b;
    }
    $$.ast = result;
  }
};
term: factor (('*'|'/'|'%'|'//') factor)* {
  D_ParseNode *list = d_get_child(&$n, 1);
  int nc = d_get_number_of_children(list);
  if (nc == 0) { $$.ast = $0.ast; }
  else {
    PyDAST *result = $0.ast;
    for (int i = 0; i < nc; i++) {
      D_ParseNode *iter = d_get_child(list, i);
      const char *op = iter->start_loc.s;
      PyDAST *right = dig_pyast(iter);
      PyDAST *b = new_pyast(PY_binop, iter);
      if (op[0] == '*') b->op = PY_OP_MUL;
      else if (op[0] == '/' && op[1] == '/') b->op = PY_OP_FLOORDIV;
      else if (op[0] == '/') b->op = PY_OP_DIV;
      else b->op = PY_OP_MOD;
      b->add(result); b->add(right);
      result = b;
    }
    $$.ast = result;
  }
};
factor: ('+'|'-'|'~') factor {
    $$.ast = new_pyast(PY_unaryop, &$n);
    char opch = $n.start_loc.s[0];
    $$.ast->op = (opch == '+') ? PY_OP_UADD : (opch == '-') ? PY_OP_USUB : PY_OP_INVERT;
    $$.ast->add($1.ast);
  }
  | power;
power: atom trailer* ('**' factor)* {
  /* Pass through if just an atom with no trailers and no ** */
  if (d_get_number_of_children(d_get_child(&$n, 1)) == 0 &&
      d_get_number_of_children(d_get_child(&$n, 2)) == 0) {
    $$.ast = $0.ast;
  } else {
    $$.ast = new_pyast(PY_power, &$n);
    dig_collect($$.ast, &$n);
  }
};
atom:
    LP (yield_expr|testlist_comp)? RP {
      /* $#==3: LP, optional inner, RP */
      D_ParseNode *q = d_get_child(&$n, 1);
      PyDAST *inner = dig_pyast(q);
      if (!inner)
        $$.ast = new_pyast(PY_tuple, &$n);  /* empty () */
      else
        $$.ast = inner;  /* tuple, genexpr, yield_expr, or single expr */
    }
  | LB listmaker? RB {
      D_ParseNode *q = d_get_child(&$n, 1);
      PyDAST *inner = dig_pyast(q);
      $$.ast = inner ? inner : new_pyast(PY_list, &$n);
    }
  | LC dictorsetmaker? RC {
      D_ParseNode *q = d_get_child(&$n, 1);
      PyDAST *inner = dig_pyast(q);
      $$.ast = inner ? inner : new_pyast(PY_dict, &$n);
    }
  | '`' testlist1 '`' {
      $$.ast = new_pyast(PY_backquote, &$n);
      $$.ast->add($1.ast);
    }
  | NAME { $$.ast = $0.ast; }
  | NUMBER {
      $$.ast = new_pyast(PY_number, &$n);
      int len = (int)($n.end - $n.start_loc.s);
      $$.ast->str_val = pyast_dupstr($n.start_loc.s, len);
    }
  | STRING+ {
      $$.ast = new_pyast(PY_string, &$n);
      int len = (int)($n.end - $n.start_loc.s);
      $$.ast->str_val = pyast_dupstr($n.start_loc.s, len);
    }
  ;
listmaker: test list_for {
    $$.ast = new_pyast(PY_listcomp, &$n);
    $$.ast->add($0.ast);
    $$.ast->add($1.ast);
  }
  | test (',' test)* ','? {
    if (d_get_number_of_children(d_get_child(&$n, 1)) == 0 &&
        d_get_number_of_children(d_get_child(&$n, 2)) == 0) {
      $$.ast = new_pyast(PY_list, &$n);
      $$.ast->add($0.ast);
    } else {
      $$.ast = new_pyast(PY_list, &$n);
      dig_collect($$.ast, &$n);
    }
  }
  ;
testlist_comp: test comp_for {
    $$.ast = new_pyast(PY_genexpr, &$n);
    $$.ast->add($0.ast);
    $$.ast->add($1.ast);
  }
  | test (',' test)* ','? {
    if (d_get_number_of_children(d_get_child(&$n, 1)) == 0 &&
        d_get_number_of_children(d_get_child(&$n, 2)) == 0) {
      $$.ast = $0.ast;  /* single expr, no tuple */
    } else {
      $$.ast = new_pyast(PY_tuple, &$n);
      dig_collect($$.ast, &$n);
    }
  }
  ;
lambdef: 'lambda' varargslist? ':' test {
  $$.ast = new_pyast(PY_lambda, &$n);
  dig_collect($$.ast, &$n);
};
trailer: LP arglist? RP {
    $$.ast = new_pyast(PY_call, &$n);
    dig_collect($$.ast, &$n);
  }
  | LB subscriptlist RB {
    $$.ast = new_pyast(PY_subscript, &$n);
    dig_collect($$.ast, &$n);
  }
  | '.' NAME {
    $$.ast = new_pyast(PY_attribute, &$n);
    $$.ast->add($1.ast);  /* the NAME node */
  }
  ;
subscriptlist: subscript (',' subscript)* ','? {
  if (d_get_number_of_children(d_get_child(&$n, 1)) == 0) {
    $$.ast = $0.ast;
  } else {
    $$.ast = new_pyast(PY_subscriptlist, &$n);
    dig_collect($$.ast, &$n);
  }
};
subscript: '.' '.' '.' { $$.ast = new_pyast(PY_slice, &$n); }
         | test
         | test? ':' test? sliceop? {
             $$.ast = new_pyast(PY_slice, &$n);
             dig_collect($$.ast, &$n);
           }
         ;
sliceop: ':' test? {
  $$.ast = new_pyast(PY_slice, &$n);
  dig_collect($$.ast, &$n);
};
exprlist: expr (',' expr)* ','? {
  if (d_get_number_of_children(d_get_child(&$n, 1)) == 0) {
    $$.ast = $0.ast;
  } else {
    $$.ast = new_pyast(PY_exprlist, &$n);
    dig_collect($$.ast, &$n);
  }
};
testlist: test (',' test)* ','? {
  if (d_get_number_of_children(d_get_child(&$n, 1)) == 0 &&
      d_get_number_of_children(d_get_child(&$n, 2)) == 0) {
    $$.ast = $0.ast;
  } else {
    $$.ast = new_pyast(PY_tuple, &$n);
    dig_collect($$.ast, &$n);
  }
};
dictorsetmaker:
    test ':' test comp_for {
      $$.ast = new_pyast(PY_dict, &$n);
      dig_collect($$.ast, &$n);
    }
  | test ':' test (',' test ':' test)* ','? {
      $$.ast = new_pyast(PY_dict, &$n);
      dig_collect($$.ast, &$n);
    }
  | test comp_for {
      $$.ast = new_pyast(PY_set, &$n);
      $$.ast->add($0.ast);
      $$.ast->add($1.ast);
    }
  | test (',' test)* ','? {
      $$.ast = new_pyast(PY_set, &$n);
      dig_collect($$.ast, &$n);
    }
  ;

classdef: 'class' NAME (LP testlist? RP)? ':' suite {
  $$.ast = new_pyast(PY_classdef, &$n);
  dig_collect($$.ast, &$n);
};

arglist: (argument ',')* (argument ','?
             | '*' test (',' argument)* (',' '**' test)?
             | '**' test) {
  $$.ast = new_pyast(PY_arglist, &$n);
  dig_collect($$.ast, &$n);
};
argument: test comp_for? {
    /* positional or with comp_for (generator in call) */
    if (d_get_number_of_children(d_get_child(&$n, 1)) == 0) {
      $$.ast = $0.ast;
    } else {
      $$.ast = new_pyast(PY_genexpr, &$n);
      dig_collect($$.ast, &$n);
    }
  }
  | test '=' test {
    $$.ast = new_pyast(PY_keyword_arg, &$n);
    $$.ast->add($0.ast);
    $$.ast->add($2.ast);
  }
  ;

list_iter: list_for | list_if;
list_for: 'for' exprlist 'in' testlist_safe list_iter? {
  $$.ast = new_pyast(PY_list_for, &$n);
  dig_collect($$.ast, &$n);
};
list_if: 'if' test list_iter? {
  $$.ast = new_pyast(PY_list_if, &$n);
  dig_collect($$.ast, &$n);
};

comp_iter: comp_for | comp_if;
comp_for: 'for' exprlist 'in' or_test comp_iter? {
  $$.ast = new_pyast(PY_comp_for, &$n);
  dig_collect($$.ast, &$n);
};
comp_if: 'if' old_test comp_iter? {
  $$.ast = new_pyast(PY_comp_if, &$n);
  dig_collect($$.ast, &$n);
};

testlist1: test (',' test)* {
  if (d_get_number_of_children(d_get_child(&$n, 1)) == 0) {
    $$.ast = $0.ast;
  } else {
    $$.ast = new_pyast(PY_testlist1, &$n);
    dig_collect($$.ast, &$n);
  }
};
encoding_decl: NAME;
yield_expr: 'yield' testlist? {
  $$.ast = new_pyast(PY_yield_expr, &$n);
  dig_collect($$.ast, &$n);
};


/* additional material from http://www.python.org/doc/current/ref/grammar.txt */

NL: '\n';
INDENT: [ if (!python_indent(&$g)) return -1; ] ;
DEDENT: [ if (!python_dedent(&$g)) return -1; ] ;
NAME: "[a-zA-Z_][a-zA-Z0-9_]*" $term -1 {
  $$.ast = new_pyast(PY_name, &$n);
  int len = (int)($n.end - $n.start_loc.s);
  $$.ast->str_val = pyast_dupstr($n.start_loc.s, len);
};
STRING ::= stringprefix?(shortstring | longstring);
shortstring ::= "'" shortstringsingleitem* "'"
| '"' shortstringdoubleitem* '"';
longstring ::= "'''" longstringitem* "'''"
| '"""' longstringitem* '"""';
shortstringsingleitem ::= shortstringsinglechar | escapeseq;
shortstringdoubleitem ::= shortstringdoublechar | escapeseq;
longstringitem ::= longstringchar | escapeseq;
shortstringsinglechar ::= "[^\\\n\']";
shortstringdoublechar ::= "[^\\\n\"]";
longstringchar ::= "[^\\]";
stringprefix ::= 'r' | 'u' | 'ur' | 'R' | 'U' | 'UR' | 'Ur' | 'uR';
escapeseq ::= "\\[^]";
NUMBER ::= integer | longinteger | floatnumber | imagnumber;
integer ::= decimalinteger | octinteger | hexinteger;
decimalinteger ::= nonzerodigit digit* | '0';
octinteger ::= '0' octdigit+;
hexinteger ::= '0' ('x' | 'X') hexdigit+;
floatnumber ::= pointfloat | exponentfloat;
pointfloat ::= intpart? fraction | intpart '.';
exponentfloat ::= (intpart | pointfloat) exponent;
intpart ::= digit+;
fraction ::= "." digit+;
exponent ::= ("e" | "E") ("+" | "-")? digit+;
imagnumber ::= (floatnumber | intpart) ("j" | "J");
longinteger ::= integer ("l" | "L");
nonzerodigit ::= "[1-9]";
digit ::= "[0-9]";
octdigit ::= "[0-7]";
hexdigit ::= digit | "[a-fA-F]";

LP ::= '(' [ $g->implicit_line_joining++; ];
RP ::= ')' [ $g->implicit_line_joining--; ];
LB ::= '[' [ $g->implicit_line_joining++; ];
RB ::= ']' [ $g->implicit_line_joining--; ];
LC ::= '{' [ $g->implicit_line_joining++; ];
RC ::= '}' [ $g->implicit_line_joining--; ];

// Default rule for single RHS non-terminals
_ : {
      if ($# == 1)
        $$.ast = $0.ast;
    };
{

#include "dparse.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

  void python_whitespace(struct D_Parser *parser, d_loc_t *loc, void **p_globals) {
    char *p = loc->s;
    PythonGlobals *pg = (PythonGlobals*)*p_globals;
    int i;
    if (!pg) {
      *p_globals = (void**)(pg = (PythonGlobals*)malloc(sizeof(PythonGlobals)));
      memset(pg, 0, sizeof(*pg));
      pg->current_indent = &pg->indent_stack[2];
    }
    if (parser->loc.s == p)
      i = 0;
    else
      i = p[-1] == '\n' ? 0 : -1;
    while (1) {
      switch (*p) {
        case '#': p++; while (*p && *p != '\n') p++; break;
        case ' ': p++; if (i >= 0) i++; break;
        case '\t': p++; if (i >= 0) i = (i + 7) & ~7; break;
        case '\n': if (i >= 0 || pg->implicit_line_joining) { loc->line++; p++; i = 0; break; }
                     // else fall through
        default: goto Ldone;
      }
    }
Ldone:;
      if (i >= 0 && !pg->implicit_line_joining && *p != '\n')
        if (i != pg->current_indent[-1]) {
          *pg->current_indent++ = i;
        }
      loc->s = p;
  }

  int python_indent(PythonGlobals **p_globals) {
    PythonGlobals *pg = *p_globals;
    if (pg) {
      if (pg->current_indent[-1] > pg->current_indent[-2])
        return 1;
      if (pg->current_indent[-1] && pg->current_indent[-1] == pg->current_indent[-2] &&
          pg->current_indent[-2] > pg->current_indent[-3]) {
        pg->current_indent--;
        return 1;
      }
    }
    return 0;
  }

  int python_dedent(PythonGlobals **p_globals) {
    int x;
    PythonGlobals *pg = *p_globals;
    if (pg && pg->current_indent[-1] < pg->current_indent[-2]) {
      pg->current_indent--;
      x = pg->current_indent[-1] = pg->current_indent[0];
      while (x == pg->current_indent[-2] && pg->current_indent > &pg->indent_stack[2])
        pg->current_indent--;
      return 1;
    }
    return 0;
  }

  /* ---- Helper functions for grammar actions ---- */

  static char *pyast_dupstr(const char *s, int len) {
    char *r = (char *)GC_MALLOC(len + 1);
    memcpy(r, s, len);
    r[len] = 0;
    return r;
  }

  static PyDAST *new_pyast(PyASTKind kind, D_ParseNode *pn) {
    PyDAST *n = new PyDAST();
    n->kind = kind;
    n->filename = pn->start_loc.pathname;
    n->line = pn->start_loc.line;
    return n;
  }

  /* Collect all non-null user.ast nodes from descendants of pn's children */
  static void dig_collect(PyDAST *parent, D_ParseNode *pn) {
    for (int i = 0; i < d_get_number_of_children(pn); i++) {
      D_ParseNode *c = d_get_child(pn, i);
      if (c->user.ast)
        parent->add(c->user.ast);
      else
        dig_collect(parent, c);
    }
  }

  /* Find the first non-null user.ast in pn or its descendants (depth-first) */
  static PyDAST *dig_pyast(D_ParseNode *pn) {
    if (pn->user.ast) return pn->user.ast;
    for (int i = 0; i < d_get_number_of_children(pn); i++) {
      PyDAST *a = dig_pyast(d_get_child(pn, i));
      if (a) return a;
    }
    return nullptr;
  }

  /* Print PyDAST for debugging */
  static const char *pyast_kind_name(PyASTKind k) {
    static const char *names[] = {
      "invalid", "module",
      "funcdef", "classdef", "decorated", "decorator",
      "suite",
      "expr_stmt", "assign", "augassign",
      "print_stmt", "del_stmt", "pass_stmt",
      "return_stmt", "break_stmt", "continue_stmt",
      "raise_stmt", "yield_stmt",
      "import_name", "import_from",
      "import_as_name", "dotted_as_name", "dotted_name",
      "global_stmt", "exec_stmt", "assert_stmt",
      "if_stmt", "elif_clause", "else_clause",
      "while_stmt", "for_stmt",
      "try_stmt", "except_clause", "except_handler", "finally_clause",
      "with_stmt", "with_item",
      "ternary", "bool_or", "bool_and", "bool_not",
      "compare", "cmp_op", "binop", "unaryop",
      "call", "attribute", "subscript", "power",
      "lambda", "yield_expr",
      "name", "number", "string", "backquote",
      "tuple", "list", "dict", "set",
      "listcomp", "genexpr",
      "slice",
      "parameters", "varargslist", "fpdef", "fplist",
      "arglist", "keyword_arg", "star_arg", "dstar_arg", "arg_default",
      "testlist", "exprlist", "testlist1",
      "comp_for", "comp_if", "list_for", "list_if",
      "subscriptlist",
    };
    if (k < 0 || k >= PY_MAX) return "?";
    return names[k];
  }

  void pyast_print(PyDAST *ast, int depth) {
    if (!ast) return;
    for (int i = 0; i < depth; i++) printf("  ");
    printf("%s", pyast_kind_name(ast->kind));
    if (ast->str_val) printf(" '%s'", ast->str_val);
    if (ast->op) printf(" op=%d", ast->op);
    printf(" [%s:%d]\n", ast->filename ? ast->filename : "?", ast->line);
    for (int i = 0; i < ast->children.n; i++)
      pyast_print(ast->children.v[i], depth + 1);
  }

}


