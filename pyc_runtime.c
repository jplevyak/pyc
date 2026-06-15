/* pyc_runtime.c — out-of-line C definitions of pyc runtime helpers
 * for the v2 LLVM backend's link step (Phase D.3.5).
 *
 * Background: pyc_c_runtime.h ships the runtime as `static inline`
 * functions + macros. The C backend (g++ over the generated .py.c)
 * inlines them per translation unit and links cleanly. The v2 LLVM
 * backend, however, produces a single .o that references those
 * helpers as external symbols — without a runtime library to link
 * against, every program that uses `__pyc_c_call__` to invoke a
 * runtime helper fails at link time.
 *
 * This file provides plain-C, externally-linkable definitions of
 * the helpers the LLVM backend calls today. The C backend is
 * unaffected: its g++ inline-derived symbols are weak/file-local,
 * so the strong externs here win on the LLVM link line without
 * collision.
 *
 * Definitions mirror pyc_c_runtime.h verbatim. Keep them in sync.
 * Macros from the header stay there — they expand inline in the
 * C backend and aren't needed for the LLVM link. C++-only
 * overloads (`_CG_prim_primitive_to_string` triple, etc.) are
 * deliberately omitted; the LLVM backend never references them by
 * name. */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "gc.h"

typedef long long int64;
typedef unsigned char _CG_bool;

#define _CG_string_len(_s) ((_s) ? (size_t) * (int64 *)(((char *)(_s)) - 8) : 0)
#define _CG_string_set_len(_s, _v) (*(int64 *)(((char *)(_s)) - 8)) = (int64)(_v)

char *_CG_string_alloc(size_t s) {
  char *str = (char *)GC_MALLOC(s + 8 + 1);
  str += 8;
  str[s] = 0;
  _CG_string_set_len(str, s);
  return str;
}

char *_CG_String(const void *x) {
  size_t len = strlen((const char *)x);
  char *str = _CG_string_alloc(len);
  memcpy(str, x, len);
  return str;
}

char *_CG_format_string(char *str, ...) {
  int l = (int)_CG_string_len(str) + 24;
  char *s = 0;
  va_list ap;
  while (1) {
    va_start(ap, str);
    s = _CG_string_alloc(l);
    int ll = vsnprintf(s, l, str, ap);
    va_end(ap);
    if (ll < l - 1) {
      _CG_string_set_len(s, ll);
      break;
    }
    l = l * 2;
  }
  return s;
}

char *_CG_str_from_int(int64 x) {
  char tmp[32];
  int n = snprintf(tmp, sizeof(tmp), "%lld", (long long)x);
  if (n < 0) n = 0;
  if ((size_t)n >= sizeof(tmp)) n = sizeof(tmp) - 1;
  char *s = _CG_string_alloc(n);
  memcpy(s, tmp, n);
  return s;
}

char *_CG_str_from_float(double d) {
  char tmp[64];
  int n = snprintf(tmp, sizeof(tmp), "%.17g", d);
  if (n < 0) n = 0;
  if ((size_t)n >= (int)sizeof(tmp)) n = sizeof(tmp) - 1;
  int has_dot_or_exp = 0;
  for (int i = 0; i < n; i++) {
    char c = tmp[i];
    if (c == '.' || c == 'e' || c == 'E' || c == 'n' || c == 'i') {
      has_dot_or_exp = 1;
      break;
    }
  }
  if (!has_dot_or_exp && n + 2 < (int)sizeof(tmp)) {
    tmp[n++] = '.';
    tmp[n++] = '0';
  }
  char *s = _CG_string_alloc(n);
  memcpy(s, tmp, n);
  return s;
}

char *_CG_string_mult(char *str, int64 n) {
  size_t l = _CG_string_len(str);
  char *ret = _CG_string_alloc(l * n);
  for (int64 i = 0; i < n; i++) memcpy(ret + l * i, str, l);
  return ret;
}

char *_CG_strcat(const char *a, const char *b) {
  size_t la = _CG_string_len(a), lb = _CG_string_len(b);
  char *x = _CG_string_alloc(la + lb);
  memcpy(x, a, la);
  memcpy(x + la, b, lb);
  return x;
}

char *_CG_char_from_string(void *s, int i) {
  char *x = _CG_string_alloc(1);
  x[0] = ((char *)s)[i];
  return x;
}

char *_CG_chr(int x) {
  unsigned char *s = (unsigned char *)_CG_string_alloc(1);
  s[0] = (unsigned char)x;
  return (char *)s;
}

int _CG_ord(char *x) {
  if (x) return *(unsigned char *)x;
  return 0;
}

_CG_bool _CG_str_eq(const char *a, const char *b) { return (_CG_bool)(strcmp(a, b) == 0); }
_CG_bool _CG_str_ne(const char *a, const char *b) { return (_CG_bool)(strcmp(a, b) != 0); }
_CG_bool _CG_str_lt(const char *a, const char *b) { return (_CG_bool)(strcmp(a, b) < 0); }
_CG_bool _CG_str_le(const char *a, const char *b) { return (_CG_bool)(strcmp(a, b) <= 0); }
_CG_bool _CG_str_gt(const char *a, const char *b) { return (_CG_bool)(strcmp(a, b) > 0); }
_CG_bool _CG_str_ge(const char *a, const char *b) { return (_CG_bool)(strcmp(a, b) >= 0); }
