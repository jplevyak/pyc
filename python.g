/* Grammar from Python 3 Grammar/Grammar, converted to dparser */

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
  int py_node_indent(struct D_Parser *p, const char *s);
  int py_next_indent(struct D_Parser *p);
  int dparser_python_user_size = sizeof(D_ParseNode_User);
  int dparser_python_globals_size = sizeof(D_ParseNode_Globals);
}

${declare longest_match}
${declare subparser single_input}
${declare subparser eval_input}
${declare whitespace python_whitespace}

file_input: (NL | stmt)* {
  $$.ast = new_pyast_collect(PY_module, &$n);
  ((PythonGlobals *)$g)->root_ast = $$.ast;
};

single_input: NL | simple_stmt | compound_stmt NL;
eval_input: testlist NL*;

decorator: '@' dotted_name ( LP arglist? RP )? NL {
  $$.ast = new_pyast_collect(PY_decorator, &$n);
};
decorators: decorator+ {
  $$.ast = new_pyast_collect(PY_suite, &$n);
};
decorated: decorators (classdef | funcdef) {
  $$.ast = new_pyast_collect(PY_decorated, &$n);
};
funcdef: 'def' NAME parameters ':' suite {
  $$.ast = new_pyast_collect(PY_funcdef, &$n);
};
parameters: LP varargslist? RP {
  $$.ast = new_pyast_collect(PY_parameters, &$n);
};
varargsarg: fpdef '=' test { $$.ast = new_pyast(PY_arg_default, &$n, $0.ast, $2.ast); }
           | fpdef { $$.ast = $0.ast; };
star_arg: '*' NAME { $$.ast = new_pyast(PY_star_arg, &$n, $1.ast); };
dstar_arg: '**' NAME { $$.ast = new_pyast(PY_dstar_arg, &$n, $1.ast); };
varargslist: varargsarg (',' varargsarg)* (',' (star_arg (',' dstar_arg)? | dstar_arg))? ','? {
  $$.ast = new_pyast_collect(PY_varargslist, &$n);
}
| star_arg (',' dstar_arg)? ','? {
  $$.ast = new_pyast_collect(PY_varargslist, &$n);
}
| dstar_arg ','? {
  $$.ast = new_pyast_collect(PY_varargslist, &$n);
};
fpdef: NAME | LP fplist RP {
  $$.ast = new_pyast_collect(PY_fpdef, &$n);
};
fplist: fpdef (',' fpdef)* ','? {
  $$.ast = new_pyast_collect(PY_fplist, &$n);
};

stmt: simple_stmt | compound_stmt;
simple_stmt: small_stmt (';' small_stmt)* ';'? NL {
  /* If only one small_stmt, pass through; else wrap in suite */
  $$.ast = pass_or_collect(1, PY_suite, &$n, $0.ast);
};
small_stmt: expr_stmt | del_stmt | pass_stmt | flow_stmt | import_stmt | global_stmt | nonlocal_stmt | assert_stmt;

expr_stmt: testlist (augassign testlist | ('=' testlist)*) {
  $$.ast = new_pyast_collect(PY_expr_stmt, &$n);
  if ($$.ast->children.n >= 2 && $$.ast->children.v[1]->kind == PY_augassign)
    $$.ast->kind = PY_augassign;
  else if ($$.ast->children.n >= 2)
    $$.ast->kind = PY_assign;
};
augassign: '+='  { $$.ast = new_op_ast(PY_augassign, &$n, PY_OP_ADD); }
         | '-='  { $$.ast = new_op_ast(PY_augassign, &$n, PY_OP_SUB); }
         | '*='  { $$.ast = new_op_ast(PY_augassign, &$n, PY_OP_MUL); }
         | '/='  { $$.ast = new_op_ast(PY_augassign, &$n, PY_OP_DIV); }
         | '%='  { $$.ast = new_op_ast(PY_augassign, &$n, PY_OP_MOD); }
         | '&='  { $$.ast = new_op_ast(PY_augassign, &$n, PY_OP_BITAND); }
         | '|='  { $$.ast = new_op_ast(PY_augassign, &$n, PY_OP_BITOR); }
         | '^='  { $$.ast = new_op_ast(PY_augassign, &$n, PY_OP_BITXOR); }
         | '<<=' { $$.ast = new_op_ast(PY_augassign, &$n, PY_OP_LSHIFT); }
         | '>>=' { $$.ast = new_op_ast(PY_augassign, &$n, PY_OP_RSHIFT); }
         | '**=' { $$.ast = new_op_ast(PY_augassign, &$n, PY_OP_POW); }
         | '//=' { $$.ast = new_op_ast(PY_augassign, &$n, PY_OP_FLOORDIV); }
         ;
del_stmt: 'del' exprlist {
  $$.ast = new_pyast_collect(PY_del_stmt, &$n);
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
  $$.ast = new_pyast_collect(PY_return_stmt, &$n);
};
raise_stmt: 'raise' (test ('from' test)?)? {
  $$.ast = new_pyast_collect(PY_raise_stmt, &$n);
};
yield_stmt: 'yield' testlist {
  $$.ast = new_pyast_collect(PY_yield_stmt, &$n);
};
import_stmt: import_name | import_from;
import_name: 'import' dotted_as_names {
  $$.ast = new_pyast_collect(PY_import_name, &$n);
};
import_from: ('from' ('.'* dotted_name | '.'+)
              'import' ('*' | '(' import_as_names ')' | import_as_names)) {
  $$.ast = new_pyast_collect(PY_import_from, &$n);
};
import_as_name: NAME ('as' NAME)? {
  $$.ast = new_pyast_collect(PY_import_as_name, &$n);
};
dotted_as_name: dotted_name ('as' NAME)? {
  $$.ast = new_pyast_collect(PY_dotted_as_name, &$n);
};
import_as_names: import_as_name (',' import_as_name)* ','? {
  $$.ast = pass_or_collect(1, PY_testlist, &$n, $0.ast);
};
dotted_as_names: dotted_as_name (',' dotted_as_name)* {
  $$.ast = pass_or_collect(1, PY_testlist, &$n, $0.ast);
};
dotted_name: NAME ('.' NAME)* {
  $$.ast = new_pyast(PY_dotted_name, &$n);
  int len = (int)($n.end - $n.start_loc.s);
  $$.ast->str_val = pyast_dupstr($n.start_loc.s, len);
};
global_stmt: 'global' NAME (',' NAME)* {
  $$.ast = new_pyast_collect(PY_global_stmt, &$n);
};
nonlocal_stmt: 'nonlocal' NAME (',' NAME)* {
  $$.ast = new_pyast_collect(PY_nonlocal_stmt, &$n);
};
assert_stmt: 'assert' test (',' test)? {
  $$.ast = new_pyast_collect(PY_assert_stmt, &$n);
};

compound_stmt: if_stmt | while_stmt | for_stmt | try_stmt | with_stmt | funcdef | classdef | decorated;

/* Named clause sub-rules for cleaner AST */
elif_clause: 'elif' test ':' suite {
  $$.ast = new_pyast_collect(PY_elif_clause, &$n);
};
else_clause: 'else' ':' suite {
  $$.ast = new_pyast_collect(PY_else_clause, &$n);
};
except_handler: except_clause ':' suite {
  $$.ast = new_pyast_collect(PY_except_handler, &$n);
};
finally_clause: 'finally' ':' suite {
  $$.ast = new_pyast_collect(PY_finally_clause, &$n);
};

if_stmt: 'if' test ':' suite elif_clause* else_clause? {
  $$.ast = new_pyast_collect(PY_if_stmt, &$n);
};
while_stmt: 'while' test ':' suite else_clause? {
  $$.ast = new_pyast_collect(PY_while_stmt, &$n);
};
for_stmt: 'for' exprlist 'in' testlist ':' suite else_clause? {
  $$.ast = new_pyast_collect(PY_for_stmt, &$n);
};
try_stmt: ('try' ':' suite
           ((except_handler)+
            else_clause?
            finally_clause? |
            finally_clause)) {
  $$.ast = new_pyast_collect(PY_try_stmt, &$n);
};
with_stmt: 'with' with_item (',' with_item)*  ':' suite {
  $$.ast = new_pyast_collect(PY_with_stmt, &$n);
};
with_item: test ('as' expr)? {
  $$.ast = new_pyast_collect(PY_with_item, &$n);
};
except_clause: 'except' (test ('as' NAME)?)? {
  $$.ast = new_pyast_collect(PY_except_clause, &$n);
};
suite: simple_stmt
     | NL INDENT suite_cmds DEDENT {
         $$.ast = new_pyast_collect(PY_suite, &$n);
       };

suite_cmds: stmt
          | suite_cmds stmt [
              if (py_node_indent(${parser}, $n0.start_loc.s) != py_node_indent(${parser}, $n1.start_loc.s))
                return -1;
            ]
          ;

test: or_test ('if' or_test 'else' test)? {
    $$.ast = pass_or_collect(1, PY_ternary, &$n, $0.ast);
  }
  | lambdef;
or_test: and_test ('or' and_test)* {
  $$.ast = pass_or_collect(1, PY_bool_or, &$n, $0.ast);
};
and_test: not_test ('and' not_test)* {
  $$.ast = pass_or_collect(1, PY_bool_and, &$n, $0.ast);
};
not_test: 'not' not_test {
    $$.ast = new_pyast(PY_bool_not, &$n, $1.ast);
  }
  | comparison;
comparison: expr (comp_op expr)* {
  $$.ast = pass_or_collect(1, PY_compare, &$n, $0.ast);
};
comp_op: '<'       { $$.ast = new_op_ast(PY_cmp_op, &$n, PY_CMP_LT); }
       | '>'       { $$.ast = new_op_ast(PY_cmp_op, &$n, PY_CMP_GT); }
       | '=='      { $$.ast = new_op_ast(PY_cmp_op, &$n, PY_CMP_EQ); }
       | '>='      { $$.ast = new_op_ast(PY_cmp_op, &$n, PY_CMP_GE); }
       | '<='      { $$.ast = new_op_ast(PY_cmp_op, &$n, PY_CMP_LE); }
       | '!='      { $$.ast = new_op_ast(PY_cmp_op, &$n, PY_CMP_NE); }
       | 'in'      { $$.ast = new_op_ast(PY_cmp_op, &$n, PY_CMP_IN); }
       | 'not' 'in'  { $$.ast = new_op_ast(PY_cmp_op, &$n, PY_CMP_NOT_IN); }
       | 'is'      { $$.ast = new_op_ast(PY_cmp_op, &$n, PY_CMP_IS); }
       | 'is' 'not' { $$.ast = new_op_ast(PY_cmp_op, &$n, PY_CMP_IS_NOT); }
       ;
expr:       xor_expr ('|'              xor_expr)* { $$.ast = build_binop_list($0.ast, &$n, op_bitor);  };
xor_expr:  and_expr ('^'              and_expr)* { $$.ast = build_binop_list($0.ast, &$n, op_bitxor); };
and_expr: shift_expr ('&'            shift_expr)* { $$.ast = build_binop_list($0.ast, &$n, op_bitand); };
shift_expr: arith_expr (('<<'|'>>') arith_expr)* { $$.ast = build_binop_list($0.ast, &$n, op_shift);  };
arith_expr:    term (('+'|'-')           term)* { $$.ast = build_binop_list($0.ast, &$n, op_arith);  };
term:        factor (('*'|'/'|'%'|'//') factor)* { $$.ast = build_binop_list($0.ast, &$n, op_term);   };
factor: ('+'|'-'|'~') factor {
    $$.ast = new_pyast(PY_unaryop, &$n);
    char opch = $n.start_loc.s[0];
    $$.ast->op = (opch == '+') ? PY_OP_UADD : (opch == '-') ? PY_OP_USUB : PY_OP_INVERT;
    $$.ast->add($1.ast);
  }
  | power;
power: atom trailer* ('**' factor)* {
  /* Pass through if just an atom with no trailers and no ** */
  $$.ast = pass_or_collect2(1, 2, PY_power, &$n, $0.ast);
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
    $$.ast = new_pyast(PY_listcomp, &$n, $0.ast, $1.ast);
  }
  | test (',' test)* ','? {
    if (d_get_number_of_children(d_get_child(&$n, 1)) == 0 &&
        d_get_number_of_children(d_get_child(&$n, 2)) == 0) {
      $$.ast = new_pyast(PY_list, &$n, $0.ast);
    } else {
      $$.ast = new_pyast_collect(PY_list, &$n);
    }
  }
  ;
testlist_comp: test comp_for {
    $$.ast = new_pyast(PY_genexpr, &$n, $0.ast, $1.ast);
  }
  | test (',' test)* ','? {
    $$.ast = pass_or_collect2(1, 2, PY_tuple, &$n, $0.ast);  /* single expr → pass through, else tuple */
  }
  ;
lambdef: 'lambda' varargslist? ':' test {
  $$.ast = new_pyast_collect(PY_lambda, &$n);
};
trailer: LP arglist? RP {
    $$.ast = new_pyast_collect(PY_call, &$n);
  }
  | LB subscriptlist RB {
    $$.ast = new_pyast_collect(PY_subscript, &$n);
  }
  | '.' NAME {
    $$.ast = new_pyast(PY_attribute, &$n, $1.ast);  /* the NAME node */
  }
  ;
subscriptlist: subscript (',' subscript)* ','? {
  $$.ast = pass_or_collect(1, PY_subscriptlist, &$n, $0.ast);
};
subscript: '.' '.' '.' { $$.ast = new_pyast(PY_slice, &$n); }
         | test
         | test? ':' test? sliceop? {
             int has_lower = (d_get_number_of_children(d_get_child(&$n, 0)) > 0);
             int has_upper = (d_get_number_of_children(d_get_child(&$n, 2)) > 0);
             $$.ast = new_pyast_collect(PY_slice, &$n);
             $$.ast->is_int = 1;
             $$.ast->int_val = has_lower | (has_upper << 1);
           }
         ;
sliceop: ':' test? {
  $$.ast = new_pyast_collect(PY_slice, &$n);
};
exprlist: expr (',' expr)* ','? {
  $$.ast = pass_or_collect(1, PY_exprlist, &$n, $0.ast);
};
testlist: test (',' test)* ','? {
  $$.ast = pass_or_collect2(1, 2, PY_tuple, &$n, $0.ast);
};
dictorsetmaker:
    test ':' test comp_for {
      $$.ast = new_pyast_collect(PY_dict, &$n);
    }
  | test ':' test (',' test ':' test)* ','? {
      $$.ast = new_pyast_collect(PY_dict, &$n);
    }
  | test comp_for {
      $$.ast = new_pyast(PY_set, &$n, $0.ast, $1.ast);
    }
  | test (',' test)* ','? {
      $$.ast = new_pyast_collect(PY_set, &$n);
    }
  ;

classdef: 'class' NAME (LP testlist? RP)? ':' suite {
  $$.ast = new_pyast_collect(PY_classdef, &$n);
};

arglist: (argument ',')* (argument ','?
             | '*' test (',' argument)* (',' '**' test)?
             | '**' test) {
  $$.ast = new_pyast_collect(PY_arglist, &$n);
};
argument: test comp_for? {
    /* positional or with comp_for (generator in call) */
    $$.ast = pass_or_collect(1, PY_genexpr, &$n, $0.ast);
  }
  | test '=' test {
    $$.ast = new_pyast(PY_keyword_arg, &$n, $0.ast, $2.ast);
  }
  ;

list_iter: list_for | list_if;
list_for: 'for' exprlist 'in' testlist list_iter? {
  $$.ast = new_pyast_collect(PY_list_for, &$n);
};
list_if: 'if' test list_iter? {
  $$.ast = new_pyast_collect(PY_list_if, &$n);
};

comp_iter: comp_for | comp_if;
comp_for: 'for' exprlist 'in' or_test comp_iter? {
  $$.ast = new_pyast_collect(PY_comp_for, &$n);
};
comp_if: 'if' or_test comp_iter? {
  $$.ast = new_pyast_collect(PY_comp_if, &$n);
};

encoding_decl: NAME;
yield_expr: 'yield' testlist? {
  $$.ast = new_pyast_collect(PY_yield_expr, &$n);
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
stringprefix ::= 'r' | 'R' | 'u' | 'U'
               | 'b' | 'B' | 'br' | 'bR' | 'Br' | 'BR' | 'rb' | 'rB' | 'Rb' | 'RB'
               | 'f' | 'F' | 'fr' | 'fR' | 'Fr' | 'FR' | 'rf' | 'rF' | 'Rf' | 'RF';
escapeseq ::= "\\[^]";
NUMBER ::= integer | floatnumber | imagnumber;
integer ::= decimalinteger | octinteger | hexinteger | bininteger;
decimalinteger ::= nonzerodigit digit* | '0';
octinteger ::= '0' ('o' | 'O') octdigit+;
hexinteger ::= '0' ('x' | 'X') hexdigit+;
bininteger ::= '0' ('b' | 'B') bindigit+;
floatnumber ::= pointfloat | exponentfloat;
pointfloat ::= intpart? fraction | intpart '.';
exponentfloat ::= (intpart | pointfloat) exponent;
intpart ::= digit+;
fraction ::= "." digit+;
exponent ::= ("e" | "E") ("+" | "-")? digit+;
imagnumber ::= (floatnumber | intpart) ("j" | "J");
nonzerodigit ::= "[1-9]";
digit ::= "[0-9]";
octdigit ::= "[0-7]";
hexdigit ::= digit | "[a-fA-F]";
bindigit ::= "[01]";

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

  int py_node_indent(struct D_Parser *p, const char *s) {
    if (!s) return -1;
    const char *x = s - 1;
    while (*x == ' ' || *x == '\t') x--;
    if (*x != '\n') return -1;
    x++;
    int i = 0;
    while (x < s) {
      if (*x == ' ') i++;
      else if (*x == '\t') i = (i + 7) & ~7;
      x++;
    }
    return i;
  }

  int py_next_indent(struct D_Parser *p) {
    if (!p->loc.s) return -1;
    return py_node_indent(p, p->loc.s);
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

  template <typename... Args>
  static PyDAST *new_pyast(PyASTKind kind, D_ParseNode *pn, Args... children) {
    PyDAST *n = new PyDAST();
    n->kind = kind;
    n->filename = pn->start_loc.pathname;
    n->line = pn->start_loc.line;
    (n->add(children), ...);
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

  static PyDAST *new_pyast_collect(PyASTKind kind, D_ParseNode *pn) {
    PyDAST *n = new_pyast(kind, pn);
    dig_collect(n, pn);
    return n;
  }

  // Pass through 'pass' if list-child at child_idx is empty, else collect into kind
  static PyDAST *pass_or_collect(int child_idx, PyASTKind kind, D_ParseNode *pn, PyDAST *pass) {
    return (d_get_number_of_children(d_get_child(pn, child_idx)) == 0)
      ? pass : new_pyast_collect(kind, pn);
  }
  // Same but both list-children at ci1 and ci2 must be empty to pass through
  static PyDAST *pass_or_collect2(int ci1, int ci2, PyASTKind kind, D_ParseNode *pn, PyDAST *pass) {
    return (d_get_number_of_children(d_get_child(pn, ci1)) == 0 &&
            d_get_number_of_children(d_get_child(pn, ci2)) == 0)
      ? pass : new_pyast_collect(kind, pn);
  }
  // Create an op-bearing leaf node (augassign or cmp_op)
  static PyDAST *new_op_ast(PyASTKind kind, D_ParseNode *pn, PyOp op) {
    PyDAST *n = new_pyast(kind, pn);
    n->op = op;
    return n;
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

  // Left-fold binary operator list into a tree of PY_binop nodes
  // op_fn selects the operator from each iteration child node
  static PyDAST *build_binop_list(PyDAST *left, D_ParseNode *pn, PyOp(*op_fn)(D_ParseNode *)) {
    D_ParseNode *list = d_get_child(pn, 1);
    int nc = d_get_number_of_children(list);
    if (nc == 0) return left;
    PyDAST *result = left;
    for (int i = 0; i < nc; i++) {
      D_ParseNode *iter = d_get_child(list, i);
      PyDAST *right = dig_pyast(iter);
      PyDAST *b = new_pyast(PY_binop, iter);
      b->op = op_fn(iter);
      b->add(result); b->add(right);
      result = b;
    }
    return result;
  }
  static PyOp op_bitor(D_ParseNode *)   { return PY_OP_BITOR; }
  static PyOp op_bitxor(D_ParseNode *)  { return PY_OP_BITXOR; }
  static PyOp op_bitand(D_ParseNode *)  { return PY_OP_BITAND; }
  static PyOp op_shift(D_ParseNode *iter) {
    return (iter->start_loc.s[0] == '<') ? PY_OP_LSHIFT : PY_OP_RSHIFT;
  }
  static PyOp op_arith(D_ParseNode *iter) {
    return (iter->start_loc.s[0] == '+') ? PY_OP_ADD : PY_OP_SUB;
  }
  static PyOp op_term(D_ParseNode *iter) {
    const char *op = iter->start_loc.s;
    if (op[0] == '*') return PY_OP_MUL;
    if (op[0] == '/' && op[1] == '/') return PY_OP_FLOORDIV;
    if (op[0] == '/') return PY_OP_DIV;
    return PY_OP_MOD;
  }

  /* Print PyDAST for debugging */
  static const char *pyast_kind_name(PyASTKind k) {
    static const char *names[] = {
      "invalid", "module",
      "funcdef", "classdef", "decorated", "decorator",
      "suite",
      "expr_stmt", "assign", "augassign",
      "del_stmt", "pass_stmt",
      "return_stmt", "break_stmt", "continue_stmt",
      "raise_stmt", "yield_stmt",
      "import_name", "import_from",
      "import_as_name", "dotted_as_name", "dotted_name",
      "global_stmt", "nonlocal_stmt", "assert_stmt",
      "if_stmt", "elif_clause", "else_clause",
      "while_stmt", "for_stmt",
      "try_stmt", "except_clause", "except_handler", "finally_clause",
      "with_stmt", "with_item",
      "ternary", "bool_or", "bool_and", "bool_not",
      "compare", "cmp_op", "binop", "unaryop",
      "call", "attribute", "subscript", "power",
      "lambda", "yield_expr",
      "name", "number", "string",
      "tuple", "list", "dict", "set",
      "listcomp", "genexpr",
      "slice",
      "parameters", "varargslist", "fpdef", "fplist",
      "arglist", "keyword_arg", "star_arg", "dstar_arg", "arg_default",
      "testlist", "exprlist",
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


