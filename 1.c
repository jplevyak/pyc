#include "./pyc_c_runtime.h"

/*
 Type Declarations
*/


/*
 Function Prototypes
*/

typedef _CG_function _CG_pf0, _CG_pf1, _CG_pf2, _CG_pf3, _CG_pf4, _CG_pf5;
_CG_bool _CG_f_299_0/*bool::__pyc_to_bool__*/(_CG_bool a1);
_CG_string _CG_f_378_1/*str::__str__*/(_CG_string a1);
_CG_string _CG_f_882_2/*int64::__str__*/(_CG_int64 a1);
_CG_bool _CG_f_1815_3/*isinstance*/(_CG_any a1);
_CG_any _CG_f_2885_4/*test_narrowing*/(_CG_bool a1);
_CG_nil_type _CG_f_105_5/*__main__*/();

/*
 Type Definitions
*/


/*
 Builtin Functions
*/


/*
 Global Variables
*/


/*
 Functions
*/

_CG_bool _CG_f_299_0/*bool::__pyc_to_bool__*/(_CG_bool a1) {
  _CG_bool t0, t1, t2;

  t1 = a1;
  t0 = t1;
  return t0;
}

_CG_string _CG_f_378_1/*str::__str__*/(_CG_string a1) {
  _CG_string t0, t1, t2;

  t1 = a1;
  t0 = t1;
  return t0;
}

_CG_string _CG_f_882_2/*int64::__str__*/(_CG_int64 a1) {
  _CG_int64 t2;
  _CG_string t0, t1, t3;

  t2 = a1;
  t1 = _CG_str_from_int(t2);
  t0 = t1;
  return t0;
}

_CG_bool _CG_f_1815_3/*isinstance*/(_CG_any a1) {
  _CG_bool t0, t1, t3;
  _CG_any t2;

  t2 = a1;
  t1 = _CG_prim_isinstance(t2, int64);
  t0 = t1;
  return t0;
}

_CG_any _CG_f_2885_4/*test_narrowing*/(_CG_bool a1) {
  _CG_bool t12, t9, t13, t7;
  _CG_int64 t1, t5, t11, t3;
  _CG_string t4, t10, t2, t6;
  _CG_any t14;
  _CG_any t8;
  _CG_any t0;

  t13 = a1;
  t12 = t13;
  if (t12) {
  t8 = (_CG_any)5;
  goto L274;
 L274:;
  t9 = _CG_f_1815_3/*isinstance*/(t8);
  t7 = t9;
  if (t7) {
  t5 = (_CG_int64)t8;
  t3 = _CG_prim_add(t5, _CG_Symbol(2164, "+"), 10);
  t1 = t3;
  t0 = (_CG_any)t1;
  goto L271;
 L271:;
  return t0;
  } else {
  t6 = (_CG_string)t8;
  t4 = _CG_prim_strcat(t6, _CG_Symbol(2131, "::"), _CG_String(" world"));
  t2 = t4;
  t0 = (_CG_any)t2;
  goto L271;
  }
  } else {
  t8 = (_CG_any)_CG_String("hi");
  goto L274;
  }
}

_CG_nil_type _CG_f_105_5/*__main__*/() {
  _CG_string t0, t2;
  _CG_any t3;
  _CG_any t1;

  t3 = _CG_f_2885_4/*test_narrowing*/(1);
  assert(!"runtime error: matching function not found");
  _CG_write(t2);
  _CG_writeln();
  t1 = _CG_f_2885_4/*test_narrowing*/(0);
  assert(!"runtime error: matching function not found");
  _CG_write(t0);
  _CG_writeln();
  return 0;
}

int main(int argc, char *argv[]) { (void)argc; (void) argv;
  MEM_INIT();
  _CG_f_105_5/*__main__*/();
  return 0;
}
