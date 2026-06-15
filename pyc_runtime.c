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

/* Pyc list header (mirrors pyc_c_runtime.h). The header sits at
 * `ptr - 16` on x86-64: total_len (uint32) at -16, len (uint32)
 * at -12, and the data pointer at -8. */
#define _CG_SIZEOF_LIST_HDR (sizeof(void *) + 8)
#define _CG_LIST_HDR_LEN(_l) (*(unsigned int *)(((char *)(_l)) - sizeof(void *) - 4))
#define _CG_LIST_HDR_TOTAL(_l) (*(unsigned int *)(((char *)(_l)) - sizeof(void *) - 8))
#define _CG_LIST_HDR_PTR(_l) (*(void **)(((char *)(_l)) - sizeof(void *)))

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

/* E.2 (issue 019): out-of-line copy of pyc's list-allocator
 * (header + payload). Inline definition lives in pyc_c_runtime.h
 * (`_CG_prim_tuple_list_internal`); the v2 LLVM backend's
 * flat-shape list-literal lowering routes here via CG2_C_CALL,
 * so libpyc_runtime.a needs to export the symbol. */
void *_CG_prim_tuple_list_internal(unsigned int element_size,
                                    unsigned int n) {
  char *base = (char *)GC_MALLOC((size_t)element_size * n + _CG_SIZEOF_LIST_HDR);
  void *result = base + _CG_SIZEOF_LIST_HDR;
  _CG_LIST_HDR_LEN(result) = n;
  _CG_LIST_HDR_TOTAL(result) = n;
  _CG_LIST_HDR_PTR(result) = result;
  return result;
}

/* E.1 (issue 019): generic struct->list conversion. Used by
 * the v2 LLVM struct-shape list-literal lowering, which
 * over-allocates a struct then hands it to this helper to get
 * the runtime-contract-shaped list (16-byte header, correct
 * header.len). Definition mirrors the static-inline copy in
 * pyc_c_runtime.h verbatim. */
void *_CG_to_list_runtime(void *struct_ptr,
                          unsigned int struct_size,
                          unsigned int semantic_n) {
  char *base = (char *)GC_MALLOC(_CG_SIZEOF_LIST_HDR + (size_t)struct_size);
  void *result = base + _CG_SIZEOF_LIST_HDR;
  _CG_LIST_HDR_LEN(result) = semantic_n;
  _CG_LIST_HDR_TOTAL(result) = semantic_n;
  _CG_LIST_HDR_PTR(result) = result;
  if (struct_ptr && struct_size > 0)
    memcpy(result, struct_ptr, struct_size);
  return result;
}

_CG_bool _CG_str_eq(const char *a, const char *b) { return (_CG_bool)(strcmp(a, b) == 0); }
_CG_bool _CG_str_ne(const char *a, const char *b) { return (_CG_bool)(strcmp(a, b) != 0); }
_CG_bool _CG_str_lt(const char *a, const char *b) { return (_CG_bool)(strcmp(a, b) < 0); }
_CG_bool _CG_str_le(const char *a, const char *b) { return (_CG_bool)(strcmp(a, b) <= 0); }
_CG_bool _CG_str_gt(const char *a, const char *b) { return (_CG_bool)(strcmp(a, b) > 0); }
_CG_bool _CG_str_ge(const char *a, const char *b) { return (_CG_bool)(strcmp(a, b) >= 0); }
