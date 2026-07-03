/* pyc_runtime.c — out-of-line C definitions of pyc runtime helpers
 * for the v2 LLVM backend's link step.
 *
 * pyc_c_runtime.h ships all shared helpers as C99 `inline` functions.
 * In C99, `inline` alone never guarantees an external symbol; this file
 * includes the header and immediately provides `extern` declarations for
 * every function the LLVM backend calls by name.  That combination
 * (C99 §6.7.4) forces the compiler to emit the external symbol here so
 * libpyc_runtime.a satisfies the LLVM linker.
 *
 * The C backend is unaffected: it compiles generated .py.c files with
 * g++ (C++ mode), where `inline` already guarantees ODR-safe emission,
 * and it never links against libpyc_runtime.a.
 *
 * LLVM-specific helpers (_CG_to_list_runtime, _CG_list_add, etc.) have
 * different signatures from the header versions (int64 element sizes
 * vs uint32) and live only here.  _CG_prim_primitive_clone_vector is
 * the canonical name; the old _CG_prim_clone_vector_runtime alias is
 * gone. */

/* Suppress the C++ overloads (_CG_prim_primitive_to_string triple and
 * _CG_float_printf) — they use C++ overloading and are already guarded
 * with #ifdef __cplusplus in the header. */
#include "pyc_c_runtime.h"

/* Force external symbol emission for every inline function the LLVM
 * backend references by name.  C99: an `extern` declaration in a TU
 * that has the inline definition in scope makes this the external
 * definition TU. */
extern char *_CG_string_alloc(size_t s);
extern char *_CG_String(const void *x);
extern char *_CG_format_string(char *str, ...);
extern char *_CG_str_from_int(int64 x);
extern char *_CG_str_from_float(double d);
extern char *_CG_string_mult(char *str, int64 n);
extern void *_CG_prim_primitive_clone_vector(void *p, size_t s, size_t v);
extern char *_CG_strcat(const char *a, const char *b);
extern char *_CG_char_from_string(void *s, int i);
extern void *_CG_prim_tuple_list_internal(unsigned int s, unsigned int n);
extern void _CG_write(const void *s);
extern void _CG_writeln(void);
extern char *_CG_chr(int x);
extern int  _CG_ord(char *x);
extern _CG_bool _CG_str_eq(const char *a, const char *b);
extern _CG_bool _CG_str_ne(const char *a, const char *b);
extern _CG_bool _CG_str_lt(const char *a, const char *b);
extern _CG_bool _CG_str_le(const char *a, const char *b);
extern _CG_bool _CG_str_gt(const char *a, const char *b);
extern _CG_bool _CG_str_ge(const char *a, const char *b);
extern _CG_int64 _CG_FFI_Alloc(_CG_int64 sz);
extern void _CG_FFI_Free(_CG_int64 p);
extern _CG_int64 _CG_FFI_Get_Int64(_CG_int64 p, _CG_int64 offset);
extern void _CG_FFI_Set_Int64(_CG_int64 p, _CG_int64 offset, _CG_int64 val);
extern _CG_int8 _CG_FFI_Get_Int8(_CG_int64 p, _CG_int64 offset);
extern void _CG_FFI_Set_Int8(_CG_int64 p, _CG_int64 offset, _CG_int8 val);

/* Pyc list header layout macros — used by the LLVM-specific list
 * helpers below.  The header uses different macro names (_CG_list_len
 * etc.) so there is no conflict. */
#define _CG_SIZEOF_LIST_HDR (sizeof(void *) + 8)
#define _CG_LIST_HDR_LEN(_l) (*(unsigned int *)(((char *)(_l)) - sizeof(void *) - 4))
#define _CG_LIST_HDR_TOTAL(_l) (*(unsigned int *)(((char *)(_l)) - sizeof(void *) - 8))
#define _CG_LIST_HDR_PTR(_l) (*(void **)(((char *)(_l)) - sizeof(void *)))

/* E.1 (issue 019): generic struct->list conversion.
 * Converts a flat struct pointer to a pyc list with the 16-byte header.
 * The C backend uses per-struct _CG_to_list overloads from the header;
 * the LLVM backend routes here via CG2_C_CALL. */
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

/* Issue 020: list runtime helpers for the LLVM backend.
 * These take int64 element sizes (matching sizeof_element's CGv2Types)
 * rather than the uint32 sizes used by the C backend's *_internal
 * counterparts in pyc_c_runtime.h.
 * The header defines same-named macros that wrap the *_internal variants;
 * undef them so the preprocessor doesn't expand our function definitions. */
#undef _CG_list_add
#undef _CG_list_resize
#undef _CG_list_mult
#undef _CG_list_getslice
#undef _CG_list_setslice

static inline unsigned int _PYC_list_len(void *l) {
  return l ? _CG_LIST_HDR_LEN(l) : 0;
}

void *_CG_list_add(void *l1, void *l2, int64 size1, int64 size2) {
  unsigned int s1 = _PYC_list_len(l1);
  unsigned int s2 = _PYC_list_len(l2);
  size_t size = (size_t)(size1 ? size1 : size2);
  void *x = GC_MALLOC(size * (s1 + s2));
  if (s1) memcpy(x, _CG_LIST_HDR_PTR(l1), (size_t)s1 * size);
  if (s2)
    memcpy(((char *)x) + (size_t)s1 * size,
           _CG_LIST_HDR_PTR(l2), (size_t)s2 * size);
  _CG_LIST_HDR_LEN(l1) = s1 + s2;
  _CG_LIST_HDR_TOTAL(l1) = s1 + s2;
  _CG_LIST_HDR_PTR(l1) = x;
  return l1;
}

void *_CG_list_resize(void *l1, int64 size1, int64 new_len) {
  unsigned int s1 = _PYC_list_len(l1);
  size_t sz = (size_t)size1;
  void *x = new_len ? GC_MALLOC(sz * (size_t)new_len) : (void *)0;
  unsigned int y = s1 < (unsigned int)new_len ? s1 : (unsigned int)new_len;
  if (y && x) memcpy(x, _CG_LIST_HDR_PTR(l1), (size_t)s1 * sz);
  if (x && (unsigned int)new_len > s1)
    memset(((char *)x) + (size_t)s1 * sz, 0,
           (size_t)((unsigned int)new_len - s1) * sz);
  _CG_LIST_HDR_LEN(l1) = (unsigned int)new_len;
  _CG_LIST_HDR_TOTAL(l1) = (unsigned int)new_len;
  _CG_LIST_HDR_PTR(l1) = x;
  return l1;
}

void *_CG_list_mult(void *l1, int64 k, int64 size) {
  if (!k) return (void *)0;
  unsigned int s1 = _PYC_list_len(l1);
  size_t sz = (size_t)size;
  char *base = (char *)GC_MALLOC(sz * s1 * (size_t)k + _CG_SIZEOF_LIST_HDR);
  void *x = base + _CG_SIZEOF_LIST_HDR;
  _CG_LIST_HDR_LEN(x) = (unsigned int)(s1 * k);
  _CG_LIST_HDR_TOTAL(x) = (unsigned int)(s1 * k);
  _CG_LIST_HDR_PTR(x) = x;
  for (int64 i = 0; i < k; i++) {
    memcpy(((char *)x) + ((size_t)i * sz * s1),
           _CG_LIST_HDR_PTR(l1), (size_t)s1 * sz);
  }
  return x;
}

void *_CG_list_getslice(void *v, int64 size, int64 l_in, int64 h_in,
                         int64 s) {
  int l = (int)l_in, h = (int)h_in;
  unsigned int len = _PYC_list_len(v);
  if (l > (int)len) l = (int)len;
  if (l < 0) { l = (int)len + l; if (l < 0) l = 0; }
  if (h > (int)len) h = (int)len;
  if (h < 0) { h = (int)len + h; if (h < 0) h = 0; }
  if (l > h) h = l;
  int span = h - l;
  int sv = (int)s;
  if (sv == 0) sv = 1;
  int n = span / sv;
  size_t sz = (size_t)size;
  char *base = (char *)GC_MALLOC(sz * (n > 0 ? n : 0) + _CG_SIZEOF_LIST_HDR);
  void *x = base + _CG_SIZEOF_LIST_HDR;
  _CG_LIST_HDR_LEN(x) = (n > 0) ? (unsigned int)n : 0;
  _CG_LIST_HDR_TOTAL(x) = _CG_LIST_HDR_LEN(x);
  _CG_LIST_HDR_PTR(x) = x;
  if (n > 0) {
    if (sv == 1) {
      memcpy(x, ((char *)_CG_LIST_HDR_PTR(v)) + (size_t)l * sz,
             (size_t)n * sz);
    } else {
      for (int i = 0; i < n; i++) {
        memcpy(((char *)x) + (size_t)i * sz,
               ((char *)_CG_LIST_HDR_PTR(v)) + (size_t)(l + i * sv) * sz,
               sz);
      }
    }
  }
  return x;
}

void *_CG_list_setslice(void *l1, int64 size, int64 l_in, int64 h_in,
                         void *l2) {
  int l = (int)l_in, h = (int)h_in;
  unsigned int len1 = _PYC_list_len(l1);
  unsigned int len2 = _PYC_list_len(l2);
  if (l > (int)len1) l = (int)len1;
  if (l < 0) { l = (int)len1 + l; if (l < 0) l = 0; }
  if (h > (int)len1) h = (int)len1;
  if (h < 0) { h = (int)len1 + h; if (h < 0) h = 0; }
  if (l > h) h = l;
  int s_del = h - l;
  int s = (int)len1 - s_del;
  int new_s = s + (int)len2;
  size_t sz = (size_t)size;
  void *p1 = _CG_LIST_HDR_PTR(l1);
  void *x = GC_MALLOC(sz * (size_t)new_s);
  _CG_LIST_HDR_LEN(l1) = (unsigned int)new_s;
  _CG_LIST_HDR_TOTAL(l1) = (unsigned int)new_s;
  _CG_LIST_HDR_PTR(l1) = x;
  char *p = (char *)x;
  if (l) { memcpy(p, p1, (size_t)l * sz); p += (size_t)l * sz; }
  if (len2) {
    memcpy(p, _CG_LIST_HDR_PTR(l2), (size_t)len2 * sz);
    p += (size_t)len2 * sz;
  }
  int sh = (int)len1 - h;
  if (sh > 0) memcpy(p, ((char *)p1) + (size_t)h * sz, (size_t)sh * sz);
  return l1;
}
