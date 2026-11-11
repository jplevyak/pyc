#define _CG_FFI_DEF extern "C"
// pyc foreign-call shims, generated from the program's ctypes declarations.
#include <stdint.h>
#include <stddef.h>
#ifndef _CG_FFI_DEF
#define _CG_FFI_DEF static inline
#endif
#define _CG_FFI_STR2(x) #x
#define _CG_FFI_STR(x) _CG_FFI_STR2(x)
#define _CG_FFI_SYM(name) __asm__(_CG_FFI_STR(__USER_LABEL_PREFIX__) name)
extern "C" size_t _CG_ffi_d_0_strlen(char *) _CG_FFI_SYM("strlen");
_CG_FFI_DEF int64_t _CG_ffi_0_strlen(int64_t a0) { return (int64_t)(_CG_ffi_d_0_strlen((char *)(intptr_t)a0)); }
extern "C" int _CG_ffi_d_0_abs(int) _CG_FFI_SYM("abs");
_CG_FFI_DEF int64_t _CG_ffi_0_abs(int64_t a0) { return (int64_t)(_CG_ffi_d_0_abs((int)a0)); }
extern "C" char * _CG_ffi_d_0_getenv(char *) _CG_FFI_SYM("getenv");
_CG_FFI_DEF int64_t _CG_ffi_0_getenv(int64_t a0) { return (int64_t)(intptr_t)(_CG_ffi_d_0_getenv((char *)(intptr_t)a0)); }
extern "C" int _CG_ffi_d_0_toupper(int) _CG_FFI_SYM("toupper");
_CG_FFI_DEF int64_t _CG_ffi_0_toupper(int64_t a0) { return (int64_t)(_CG_ffi_d_0_toupper((int)a0)); }
extern "C" int _CG_ffi_d_0_atoi(char *) _CG_FFI_SYM("atoi");
_CG_FFI_DEF int64_t _CG_ffi_0_atoi(int64_t a0) { return (int64_t)(_CG_ffi_d_0_atoi((char *)(intptr_t)a0)); }
extern "C" void _CG_ffi_d_0_srand(unsigned int) _CG_FFI_SYM("srand");
_CG_FFI_DEF int64_t _CG_ffi_0_srand(int64_t a0) { _CG_ffi_d_0_srand((unsigned int)a0); return 0; }
extern "C" double _CG_ffi_d_1_cbrt(double) _CG_FFI_SYM("cbrt");
_CG_FFI_DEF double _CG_ffi_1_cbrt(double a0) { return (double)(_CG_ffi_d_1_cbrt((double)a0)); }
extern "C" double _CG_ffi_d_1_ldexp(double, int) _CG_FFI_SYM("ldexp");
_CG_FFI_DEF double _CG_ffi_1_ldexp(double a0, int64_t a1) { return (double)(_CG_ffi_d_1_ldexp((double)a0, (int)a1)); }
extern "C" float _CG_ffi_d_1_fabsf(float) _CG_FFI_SYM("fabsf");
_CG_FFI_DEF double _CG_ffi_1_fabsf(double a0) { return (double)(_CG_ffi_d_1_fabsf((float)a0)); }
extern "C" bool _CG_ffi_d_2_abs(int) _CG_FFI_SYM("abs");
_CG_FFI_DEF int64_t _CG_ffi_2_abs(int64_t a0) { return (int64_t)(_CG_ffi_d_2_abs((int)a0)); }
