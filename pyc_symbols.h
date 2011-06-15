/*
  Copyright 2011 John Plevyak, All Rights Reserved

  S is a builtin symbol.
  P is for "public" (scoped) symbols a subset of S.
  B is for builtin function (implemented in the compiler frontend) a subset of P.
*/
#ifndef S
#define S(_x)
#endif
#ifndef P
#define P(_x) S(_x)
#endif
#ifndef B
#define B(_x) P(_x)
#endif
S(write)
S(writeln)
S(__iter__)
S(next)
S(append)
S(__new__)
S(__getitem__)
S(__setitem__)
S(__getslice__)
S(__setslice__)
S(__init__)
S(__call__)
S(__null__)
S(__str__)
S(__pyc_setslice__)
S(__pyc_getslice__)
P(__pyc_more__)
P(__pyc_to_bool__)
P(__pyc_to_str__)
P(__pyc_format_string__)
B(super)
B(__pyc_symbol__)
B(__pyc_clone_constants__)
B(__pyc_c_call__)
B(__pyc_c_code__)
B(__pyc_insert_c_header__)
B(__pyc_insert_c_code__)
B(__pyc_include_c_header__)
#undef S
#undef P
#undef B
