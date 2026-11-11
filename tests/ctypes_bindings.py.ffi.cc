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
struct _CG_ffi_s_0;
struct _CG_ffi_s_0 {
  int quot;
  int rem;
};
_CG_FFI_DEF int64_t _CG_ffi_sizeof_0(void) { return (int64_t)sizeof(struct _CG_ffi_s_0); }
_CG_FFI_DEF int64_t _CG_ffi_get_0_quot(int64_t a) { return (int64_t)(((struct _CG_ffi_s_0 *)(intptr_t)a)->quot); }
_CG_FFI_DEF int64_t _CG_ffi_set_0_quot(int64_t a, int64_t v) { ((struct _CG_ffi_s_0 *)(intptr_t)a)->quot = (int)v; return 0; }
_CG_FFI_DEF int64_t _CG_ffi_get_0_rem(int64_t a) { return (int64_t)(((struct _CG_ffi_s_0 *)(intptr_t)a)->rem); }
_CG_FFI_DEF int64_t _CG_ffi_set_0_rem(int64_t a, int64_t v) { ((struct _CG_ffi_s_0 *)(intptr_t)a)->rem = (int)v; return 0; }
extern "C" double _CG_ffi_d_0_hypot(double, double) _CG_FFI_SYM("hypot");
_CG_FFI_DEF double _CG_ffi_0_hypot(double a0, double a1) { return (double)(_CG_ffi_d_0_hypot((double)a0, (double)a1)); }
extern "C" struct _CG_ffi_s_0 _CG_ffi_d_1_div(int, int) _CG_FFI_SYM("div");
_CG_FFI_DEF int64_t _CG_ffi_1_div(int64_t r, int64_t a0, int64_t a1) { *(struct _CG_ffi_s_0 *)(intptr_t)r = _CG_ffi_d_1_div((int)a0, (int)a1); return 0; }
extern "C" size_t _CG_ffi_d_1_strlen(char *) _CG_FFI_SYM("strlen");
_CG_FFI_DEF int64_t _CG_ffi_1_strlen(int64_t a0) { return (int64_t)(_CG_ffi_d_1_strlen((char *)(intptr_t)a0)); }
