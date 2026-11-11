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
struct _CG_ffi_s_1;
struct _CG_ffi_s_2;
struct _CG_ffi_s_3;
struct _CG_ffi_s_0 {
  double x;
  double y;
  int tag;
  char * name;
};
_CG_FFI_DEF int64_t _CG_ffi_sizeof_0(void) { return (int64_t)sizeof(struct _CG_ffi_s_0); }
_CG_FFI_DEF double _CG_ffi_get_0_x(int64_t a) { return (double)(((struct _CG_ffi_s_0 *)(intptr_t)a)->x); }
_CG_FFI_DEF int64_t _CG_ffi_set_0_x(int64_t a, double v) { ((struct _CG_ffi_s_0 *)(intptr_t)a)->x = (double)v; return 0; }
_CG_FFI_DEF double _CG_ffi_get_0_y(int64_t a) { return (double)(((struct _CG_ffi_s_0 *)(intptr_t)a)->y); }
_CG_FFI_DEF int64_t _CG_ffi_set_0_y(int64_t a, double v) { ((struct _CG_ffi_s_0 *)(intptr_t)a)->y = (double)v; return 0; }
_CG_FFI_DEF int64_t _CG_ffi_get_0_tag(int64_t a) { return (int64_t)(((struct _CG_ffi_s_0 *)(intptr_t)a)->tag); }
_CG_FFI_DEF int64_t _CG_ffi_set_0_tag(int64_t a, int64_t v) { ((struct _CG_ffi_s_0 *)(intptr_t)a)->tag = (int)v; return 0; }
_CG_FFI_DEF int64_t _CG_ffi_get_0_name(int64_t a) { return (int64_t)(intptr_t)(((struct _CG_ffi_s_0 *)(intptr_t)a)->name); }
_CG_FFI_DEF int64_t _CG_ffi_set_0_name(int64_t a, int64_t v) { ((struct _CG_ffi_s_0 *)(intptr_t)a)->name = (char *)(intptr_t)v; return 0; }
struct _CG_ffi_s_1 {
  int quot;
  int rem;
};
_CG_FFI_DEF int64_t _CG_ffi_sizeof_1(void) { return (int64_t)sizeof(struct _CG_ffi_s_1); }
_CG_FFI_DEF int64_t _CG_ffi_get_1_quot(int64_t a) { return (int64_t)(((struct _CG_ffi_s_1 *)(intptr_t)a)->quot); }
_CG_FFI_DEF int64_t _CG_ffi_set_1_quot(int64_t a, int64_t v) { ((struct _CG_ffi_s_1 *)(intptr_t)a)->quot = (int)v; return 0; }
_CG_FFI_DEF int64_t _CG_ffi_get_1_rem(int64_t a) { return (int64_t)(((struct _CG_ffi_s_1 *)(intptr_t)a)->rem); }
_CG_FFI_DEF int64_t _CG_ffi_set_1_rem(int64_t a, int64_t v) { ((struct _CG_ffi_s_1 *)(intptr_t)a)->rem = (int)v; return 0; }
struct _CG_ffi_s_2 {
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
  long tm_gmtoff;
  char * tm_zone;
};
_CG_FFI_DEF int64_t _CG_ffi_sizeof_2(void) { return (int64_t)sizeof(struct _CG_ffi_s_2); }
_CG_FFI_DEF int64_t _CG_ffi_get_2_tm_sec(int64_t a) { return (int64_t)(((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_sec); }
_CG_FFI_DEF int64_t _CG_ffi_set_2_tm_sec(int64_t a, int64_t v) { ((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_sec = (int)v; return 0; }
_CG_FFI_DEF int64_t _CG_ffi_get_2_tm_min(int64_t a) { return (int64_t)(((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_min); }
_CG_FFI_DEF int64_t _CG_ffi_set_2_tm_min(int64_t a, int64_t v) { ((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_min = (int)v; return 0; }
_CG_FFI_DEF int64_t _CG_ffi_get_2_tm_hour(int64_t a) { return (int64_t)(((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_hour); }
_CG_FFI_DEF int64_t _CG_ffi_set_2_tm_hour(int64_t a, int64_t v) { ((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_hour = (int)v; return 0; }
_CG_FFI_DEF int64_t _CG_ffi_get_2_tm_mday(int64_t a) { return (int64_t)(((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_mday); }
_CG_FFI_DEF int64_t _CG_ffi_set_2_tm_mday(int64_t a, int64_t v) { ((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_mday = (int)v; return 0; }
_CG_FFI_DEF int64_t _CG_ffi_get_2_tm_mon(int64_t a) { return (int64_t)(((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_mon); }
_CG_FFI_DEF int64_t _CG_ffi_set_2_tm_mon(int64_t a, int64_t v) { ((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_mon = (int)v; return 0; }
_CG_FFI_DEF int64_t _CG_ffi_get_2_tm_year(int64_t a) { return (int64_t)(((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_year); }
_CG_FFI_DEF int64_t _CG_ffi_set_2_tm_year(int64_t a, int64_t v) { ((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_year = (int)v; return 0; }
_CG_FFI_DEF int64_t _CG_ffi_get_2_tm_wday(int64_t a) { return (int64_t)(((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_wday); }
_CG_FFI_DEF int64_t _CG_ffi_set_2_tm_wday(int64_t a, int64_t v) { ((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_wday = (int)v; return 0; }
_CG_FFI_DEF int64_t _CG_ffi_get_2_tm_yday(int64_t a) { return (int64_t)(((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_yday); }
_CG_FFI_DEF int64_t _CG_ffi_set_2_tm_yday(int64_t a, int64_t v) { ((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_yday = (int)v; return 0; }
_CG_FFI_DEF int64_t _CG_ffi_get_2_tm_isdst(int64_t a) { return (int64_t)(((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_isdst); }
_CG_FFI_DEF int64_t _CG_ffi_set_2_tm_isdst(int64_t a, int64_t v) { ((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_isdst = (int)v; return 0; }
_CG_FFI_DEF int64_t _CG_ffi_get_2_tm_gmtoff(int64_t a) { return (int64_t)(((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_gmtoff); }
_CG_FFI_DEF int64_t _CG_ffi_set_2_tm_gmtoff(int64_t a, int64_t v) { ((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_gmtoff = (long)v; return 0; }
_CG_FFI_DEF int64_t _CG_ffi_get_2_tm_zone(int64_t a) { return (int64_t)(intptr_t)(((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_zone); }
_CG_FFI_DEF int64_t _CG_ffi_set_2_tm_zone(int64_t a, int64_t v) { ((struct _CG_ffi_s_2 *)(intptr_t)a)->tm_zone = (char *)(intptr_t)v; return 0; }
extern "C" struct _CG_ffi_s_1 _CG_ffi_d_0_div(int, int) _CG_FFI_SYM("div");
_CG_FFI_DEF int64_t _CG_ffi_0_div(int64_t r, int64_t a0, int64_t a1) { *(struct _CG_ffi_s_1 *)(intptr_t)r = _CG_ffi_d_0_div((int)a0, (int)a1); return 0; }
extern "C" long _CG_ffi_d_0_timegm(struct _CG_ffi_s_2 *) _CG_FFI_SYM("timegm");
_CG_FFI_DEF int64_t _CG_ffi_0_timegm(int64_t a0) { return (int64_t)(_CG_ffi_d_0_timegm((struct _CG_ffi_s_2 *)(intptr_t)a0)); }
extern "C" struct _CG_ffi_s_3 * _CG_ffi_d_0_fopen(char *, char *) _CG_FFI_SYM("fopen");
_CG_FFI_DEF int64_t _CG_ffi_0_fopen(int64_t a0, int64_t a1) { return (int64_t)(intptr_t)(_CG_ffi_d_0_fopen((char *)(intptr_t)a0, (char *)(intptr_t)a1)); }
extern "C" int _CG_ffi_d_0_fclose(struct _CG_ffi_s_3 *) _CG_FFI_SYM("fclose");
_CG_FFI_DEF int64_t _CG_ffi_0_fclose(int64_t a0) { return (int64_t)(_CG_ffi_d_0_fclose((struct _CG_ffi_s_3 *)(intptr_t)a0)); }
