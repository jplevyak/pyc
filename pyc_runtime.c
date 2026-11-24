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
#include <dlfcn.h>
#include <limits.h>

/* Force external symbol emission for every inline function the LLVM
 * backend references by name.  C99: an `extern` declaration in a TU
 * that has the inline definition in scope makes this the external
 * definition TU. */
extern char *_CG_string_alloc(size_t s);
extern char *_CG_String(const void *x);
extern _CG_int64 _CG_ffi_bytes_addr(char *b);
extern char *_CG_ffi_cstr(_CG_int64 p);
extern char *_CG_ffi_buf(_CG_int64 n);
extern char *_CG_ffi_char(_CG_int64 v);
extern _CG_int64 _CG_ffi_cb_ignored(char *msg);
extern _CG_int64 _CG_ffi_copy(_CG_int64 dst, _CG_int64 src, _CG_int64 n);
extern char *_CG_ffi_rd_cstr_n(_CG_int64 a, _CG_int64 n);
extern char *_CG_ffi_rd_raw(_CG_int64 a, _CG_int64 n);
extern _CG_int64 _CG_ffi_wr_chars(_CG_int64 a, char *b, _CG_int64 len, _CG_int64 n);
extern _CG_int64 _CG_ffi_sizeof_ptr(void);
extern _CG_int64 _CG_ffi_rd_ptr(_CG_int64 a);
extern _CG_int64 _CG_ffi_wr_ptr(_CG_int64 a, _CG_int64 v);
extern char *_CG_format_string(char *str, ...);
extern char *_CG_format_string_tagged(char *str, const char *tags, ...);
extern char *_CG_format_convs(const char *fmt);
extern int64 _CG_str_is_ascii(const char *s);
extern char *_CG_fmt_grow(char *out, size_t *cap, size_t len, size_t need);
extern const char *_CG_widen_int_convs(const char *fmt);
extern char *_CG_str_from_int(int64 x);
extern long long _CG_hash_mix(long long h, long long x);
extern char *_CG_str_from_float(double d);
extern double _CG_str_to_float64(char *s);
extern int64 _CG_str_to_int64(char *s);
extern int64 _CG_str_to_int64_base(char *s, int base);
/* issues/176: whole-string validation for int() / float() of a str. */
extern int64 _CG_str_int_ok(char *s, int64 base);
extern int64 _CG_str_int_value(char *s, int64 base);
extern int64 _CG_str_float_ok(char *s);
extern double _CG_str_float_value(char *s);
extern int64 _CG_float_to_int64(double x);
extern char *_CG_group_digits(const char *core, char sep);
extern char *_CG_pad_align(const char *core, int width, char align, char fill, int sign_len);
extern void  _CG_parse_format_spec(const char *spec, _CG_FormatSpec *out);
extern char *_CG_format_int_spec(int64 val, const char *spec_str);
extern char *_CG_format_float_spec(double val, const char *spec_str);
extern char *_CG_format_str_spec(const char *val, const char *spec_str);
extern char *_CG_string_mult(char *str, int64 n);
extern char *_CG_string_identity(char *s);
extern void *_CG_prim_primitive_clone_vector(void *p, size_t s, size_t v);
/* P_prim_copy of a receiver whose static type is a union of record
 * CreationSets (genetic2's TreeNode.__deepcopy__): sized at run time. */
extern void *_CG_prim_copy_any(void *p);
extern char *_CG_strcat(const char *a, const char *b);
extern void _CG_none_receiver(const char *sel);  // ifa/issues/165
extern char *_CG_string_join(const char *sep, _CG_list parts);
extern char *_CG_str_casemap(const char *s, int mode);  // issues/050
extern char *_CG_str_lower(const char *s);
extern char *_CG_str_upper(const char *s);
extern char *_CG_str_swapcase(const char *s);
extern char *_CG_str_substr(const char *s, int64 i, int64 j);
extern char *_CG_str_replace(const char *s, const char *old, const char *nw);
extern int64 _CG_str_find(const char *s, const char *sub, int64 i, int64 e);
extern int64 _CG_str_rfind(const char *s, const char *sub, int64 i, int64 e);
extern char *_CG_one_char_string(unsigned char c);
extern int64 _CG_int_pow(int64 b, int64 e);
extern int64 _CG_errno(void);
extern char *_CG_locale_encoding(void);
extern int64 _CG_entropy32(void);
extern int64 _CG_sleep(double seconds);  /* issues/041 */
extern char *_CG_gethostname(void);
extern char *_CG_strftime_local(char *fmt, double t);
extern int64 _CG_int_bit_count(int64 x);
extern char *_CG_strerror(int64 e);
extern char *_CG_char_from_string(void *s, int i);
extern int64 _CG_int_from_string(void *s, int i);
extern char *_CG_byte_from_int(int64 v);
extern char *_CG_str_repr(const char *s);
extern int32 _CG_norm_idx(int32 idx, int32 len);
extern char *_CG_string_getslice(const char *s, int32 l, int32 h, int32 step);
extern void *_CG_prim_tuple_list_internal(unsigned int s, unsigned int n);
extern void _CG_write(const void *s);
extern void _CG_writeln(void);
extern char *_CG_chr(int x);
extern int  _CG_ord(char *x);
extern _CG_bool _CG_str_eq(const char *a, const char *b);
extern _CG_bool _CG_str_ne(const char *a, const char *b);
extern long long _CG_str_hash(const char *s);
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
extern int64 _CG_fopen(char *path, char *mode);
extern int64 _CG_fstd(int64 which);
extern int64 _CG_fclose(int64 h);
extern int64 _CG_fflush(int64 h);
extern int64 _CG_fseek(int64 h, int64 off, int64 whence);
extern int64 _CG_ftell(int64 h);
extern int64 _CG_fwrite_str(int64 h, char *s);
extern char *_CG_fread_all(int64 h);
extern char *_CG_fread_n(int64 h, int64 n);
extern char *_CG_freadline(int64 h);
extern void _CG_set_argv(int64 argc, char **argv);
extern int64 _CG_argc(void);
extern char *_CG_argv_at(int64 i);
extern char *_CG_getcwd(void);
extern _CG_bool _CG_is_dir(const char *path);
extern _CG_bool _CG_is_symlink(const char *path);
extern int64 _CG_stat_int_field(const char *path, int64 field);
extern int64 _CG_opendir(const char *path);
extern int64 _CG_closedir(int64 h);
extern char *_CG_readdir_name(int64 h);
extern double _CG_frexp_m(double x);
extern int64 _CG_frexp_e(double x);
extern double _CG_modf_frac(double x);
extern double _CG_modf_int(double x);
extern int64 _CG_int_floordiv(int64 a, int64 b);
extern double _CG_float_floordiv(double vx, double wx);
extern double _CG_round_even(double x);
extern double _CG_round_ndigits(double x, int64 nd);
extern int64 _CG_clock_ns(int64 which);
extern int64 _CG_time_tm(int64 secs, int64 local);
extern int64 _CG_time_tm_field(int64 i);
extern char *_CG_time_tm_zone(void);
extern char *_CG_strftime_fields(char *fmt, int64 year, int64 mon, int64 mday, int64 hour, int64 min, int64 sec,
                                 int64 wday, int64 yday, int64 isdst, int64 gmtoff, char *zone);
extern double _CG_mktime_fields(int64 year, int64 mon, int64 mday, int64 hour, int64 min, int64 sec, int64 wday,
                                int64 yday, int64 isdst);
extern int64 _CG_os_err(void);
extern int64 _CG_os_mkdir(const char *p, int64 mode);
extern int64 _CG_os_rmdir(const char *p);
extern int64 _CG_os_unlink(const char *p);
extern int64 _CG_os_chdir(const char *p);
extern int64 _CG_os_rename(const char *a, const char *b);
extern int64 _CG_os_symlink(const char *a, const char *b);
extern int64 _CG_os_access(const char *p, int64 mode);
extern int64 _CG_os_utime_now(const char *p);
extern int64 _CG_os_create(const char *p, int64 mode, int64 excl);
extern int64 _CG_os_cpu_count(void);
extern int64 _CG_os_ignorable_errno(int64 e);
extern int64 _CG_os_stat(const char *p, int64 follow);
extern int64 _CG_os_stat_int(int64 i);
extern double _CG_os_stat_time(int64 i);
extern char *_CG_os_environ_at(int64 i);
extern int64 _CG_os_setenv(const char *k, const char *v);
extern int64 _CG_os_unsetenv(const char *k);
extern char *_CG_os_readlink(const char *p);
extern char *_CG_os_pw_home(const char *name);

/* Real process argv storage -- definition (extern-declared in
 * pyc_c_runtime.h), same pattern as _CG_ready_queue_head below: one
 * copy shared by both backends, set once from generated main() via
 * _CG_set_argv before anything else runs. */
int64 _cg_argc = 0;
char **_cg_argv = 0;
int _cg_os_err = 0;
struct tm _cg_time_tm;
struct stat _cg_os_st;

/* ifa/issues/051: GC initialisation for the LLVM backend.
 *
 * The C backend's generated main() opens with MEM_INIT() -> GC_INIT();
 * the LLVM backend's generated main() did not initialise the collector
 * at all, leaving Boehm to come up lazily on the first allocation. The
 * part that matters is GC_INIT_CONF_ROOTS, which registers the static
 * data segment (GC_DATASTART..GC_DATAEND) as roots -- and that is
 * exactly where the LLVM backend puts its module globals (@states,
 * @cur_state, ...). Without it those globals are not scanned, so an
 * object reachable ONLY from a global is collected while still live;
 * the program then writes through the stale pointer and corrupts
 * Boehm's free list, which surfaces as a segfault inside
 * GC_finish_collection far from the actual fault.
 *
 * Confirmed: the affected program runs correctly under GC_DONT_GC=1 or
 * a heap large enough that no collection happens. */
void _CG_gc_init(void) { GC_INIT(); }

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
#undef _CG_tuple_getslice
#undef _CG_list_setslice
#undef _CG_list_delslice

static inline unsigned int _PYC_list_len(void *l) {
  return l ? _CG_LIST_HDR_LEN(l) : 0;
}

/* issues/044: was writing the concatenated result into l1's own
 * header and returning l1 -- correct for list.append()'s in-place
 * contract (_CG_list_resize below) but wrong for list.__add__ (`+`),
 * which CPython never lets mutate either operand. Any receiver
 * aliased elsewhere (an object field, another variable) silently
 * gained the concatenation as a side effect of merely evaluating
 * `+`. Fresh-allocate the header too, mirroring _CG_list_getslice's
 * pattern just below (already correct for `list[a:b]`). */
void *_CG_list_add(void *l1, void *l2, int64 size1, int64 size2) {
  unsigned int s1 = _PYC_list_len(l1);
  unsigned int s2 = _PYC_list_len(l2);
  size_t size = (size_t)(size1 ? size1 : size2);
  char *base = (char *)GC_MALLOC(size * (s1 + s2) + _CG_SIZEOF_LIST_HDR);
  void *x = base + _CG_SIZEOF_LIST_HDR;
  _CG_LIST_HDR_LEN(x) = s1 + s2;
  _CG_LIST_HDR_TOTAL(x) = s1 + s2;
  _CG_LIST_HDR_PTR(x) = x;
  if (s1) memcpy(_CG_LIST_HDR_PTR(x), _CG_LIST_HDR_PTR(l1), (size_t)s1 * size);
  if (s2)
    memcpy(((char *)_CG_LIST_HDR_PTR(x)) + (size_t)s1 * size,
           _CG_LIST_HDR_PTR(l2), (size_t)s2 * size);
  return x;
}

// Mirrors pyc_c_runtime.h's _CG_list_resize_internal (issues/044), which
// this LLVM-backend copy had drifted from twice:
//   - it reallocated to EXACTLY new_len on every call, so list.append was
//     O(n) and building a list O(n^2) -- shedskin_examples/sha's LLVM
//     binary spent minutes in the collector growing a 143k-element list
//     one slot at a time, where the C backend (amortized) takes 17 s;
//   - it copied `s1` elements even when SHRINKING, writing past the end of
//     the new buffer -- the bug the header fixed with `y * size1`.
void *_CG_list_resize(void *l1, int64 size1, int64 new_len) {
  unsigned int s1 = _PYC_list_len(l1);
  unsigned int cap = l1 ? (_CG_LIST_HDR_TOTAL(l1) & ~_CG_TUPLE_FIXED_ARITY) : 0;  // ifa/185
  size_t sz = (size_t)size1;
  if (new_len && (unsigned int)new_len <= cap && _CG_LIST_HDR_PTR(l1)) {
    // Already big enough: move the length, zero anything newly exposed.
    if ((unsigned int)new_len > s1)
      memset(((char *)_CG_LIST_HDR_PTR(l1)) + (size_t)s1 * sz, 0, (size_t)((unsigned int)new_len - s1) * sz);
    _CG_LIST_HDR_LEN(l1) = (unsigned int)new_len;
    return l1;
  }
  unsigned int newcap = cap < 4 ? 4 : cap * 2;
  if (newcap < (unsigned int)new_len) newcap = (unsigned int)new_len;
  void *x = new_len ? GC_MALLOC(sz * (size_t)newcap) : (void *)0;
  unsigned int y = s1 < (unsigned int)new_len ? s1 : (unsigned int)new_len;
  if (y && x) memcpy(x, _CG_LIST_HDR_PTR(l1), (size_t)y * sz);
  if (x && (unsigned int)new_len > s1)
    memset(((char *)x) + (size_t)s1 * sz, 0, (size_t)((unsigned int)new_len - s1) * sz);
  _CG_LIST_HDR_LEN(l1) = (unsigned int)new_len;
  _CG_LIST_HDR_TOTAL(l1) = new_len ? newcap : 0;
  _CG_LIST_HDR_PTR(l1) = x;
  return l1;
}

// A NEW list even for k <= 0; see _CG_list_mult_internal in
// pyc_c_runtime.h for why NULL is wrong here.
void *_CG_list_mult(void *l1, int64 k, int64 size) {
  if (k < 0) k = 0;
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

// See pyc_c_runtime.h's _CG_list_getslice_internal/_CG_string_getslice
// for the sentinel (INT_MIN/INT_MAX omitted-bound) and negative-step
// algorithm this mirrors -- CPython's PySlice_GetIndicesEx. `s[::-1]`
// used to compute a negative element count that, stored into the
// (unsigned) list-length header, wrapped to a huge value and hung on
// read/print.
void *_CG_list_getslice(void *v, int64 size, int64 l_in, int64 h_in,
                         int64 s) {
  int l = (int)l_in, h = (int)h_in;
  int len = (int)_PYC_list_len(v);
  int sv = (int)s;
  if (sv == 0) sv = 1;
  if (l == INT32_MIN) {
    l = sv < 0 ? len - 1 : 0;
  } else if (l < 0) {
    l += len;
    if (l < 0) l = sv < 0 ? -1 : 0;
  } else if (l >= len) {
    l = sv < 0 ? len - 1 : len;
  }
  if (h == INT32_MAX) {
    h = sv < 0 ? -1 : len;
  } else if (h < 0) {
    h += len;
    if (h < 0) h = sv < 0 ? -1 : 0;
  } else if (h >= len) {
    h = sv < 0 ? len - 1 : len;
  }
  int n;
  if (sv > 0)
    n = l < h ? (h - l + sv - 1) / sv : 0;
  else
    n = l > h ? (l - h + (-sv) - 1) / (-sv) : 0;
  if (n < 0) n = 0;
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

/* ifa/102: a tuple slice is a dynamic tuple -- the list slicer, with the
 * header's _CG_TUPLE_DYNAMIC bit set (pyc_c_runtime.h). */
void *_CG_tuple_getslice(void *v, int64 size, int64 l_in, int64 h_in, int64 s) {
  void *x = _CG_list_getslice(v, size, l_in, h_in, s);
  _CG_LIST_HDR_TOTAL(x) |= 0x40000000u;
  return x;
}

/* issues/166: the step, which this used to have no parameter for -- see
 * _CG_list_setslice_internal in pyc_c_runtime.h for the full rationale and
 * the CPython semantics. Kept in sync with it deliberately, the way every
 * other helper in this file mirrors its inline twin. */
void *_CG_list_setslice(void *l1, int64 size, int64 l_in, int64 h_in,
                         int64 st_in, void *l2) {
  /* `st` (step), not `s`: the contiguous path below already uses `s` for a
   * byte count. */
  int l = (int)l_in, h = (int)h_in, st = (int)st_in;
  unsigned int len1 = _PYC_list_len(l1);
  unsigned int len2 = _PYC_list_len(l2);
  if (!st) st = 1;
  if (st != 1) {
    int ilen1 = (int)len1, n;
    if (l == INT32_MIN) {
      l = st < 0 ? ilen1 - 1 : 0;
    } else if (l < 0) {
      l += ilen1;
      if (l < 0) l = st < 0 ? -1 : 0;
    } else if (l >= ilen1) {
      l = st < 0 ? ilen1 - 1 : ilen1;
    }
    if (h == INT32_MAX) {
      h = st < 0 ? -1 : ilen1;
    } else if (h < 0) {
      h += ilen1;
      if (h < 0) h = st < 0 ? -1 : 0;
    } else if (h >= ilen1) {
      h = st < 0 ? ilen1 - 1 : ilen1;
    }
    if (st > 0)
      n = l < h ? (h - l + st - 1) / st : 0;
    else
      n = l > h ? (l - h + (-st) - 1) / (-st) : 0;
    if (n < 0) n = 0;
    if (n != (int)len2) {
      assert(!"runtime error: attempt to assign a sequence of the wrong size to an extended slice");
      return l1;
    }
    {
      char *dst = (char *)_CG_LIST_HDR_PTR(l1);
      char *src = (char *)_CG_LIST_HDR_PTR(l2);
      int i;
      for (i = 0; i < n; i++)
        memcpy(dst + (size_t)(l + i * st) * (size_t)size, src + (size_t)i * (size_t)size, (size_t)size);
    }
    return l1;
  }
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

/* issues/166: `del a[i:j:k]` -- the LLVM twin of
 * _CG_list_delslice_internal in pyc_c_runtime.h; see that function for
 * the CPython semantics and why a strided DELETE is a different
 * operation from the extended STORE above (it removes the selected
 * elements and shrinks; an extended store may not resize at all).
 * Kept in sync with it deliberately, the way every other helper in this
 * file mirrors its inline twin. */
void *_CG_list_delslice(void *l1, int64 size, int64 l_in, int64 h_in, int64 st_in) {
  int l = (int)l_in, h = (int)h_in, st = (int)st_in;
  int len1 = (int)_PYC_list_len(l1);
  size_t sz = (size_t)size;
  int n, start, step, w, k, next, r;
  char *p;
  if (!st) st = 1;
  if (st == 1) {
    /* Contiguous: the ordinary splice with nothing inserted. Same clamp
     * order as _CG_list_setslice's k == 1 path, deliberately -- the
     * strided path below uses _CG_list_getslice's normalisation instead. */
    void *p1;
    void *x;
    int s_del, s_keep, sh;
    if (l > len1) l = len1;
    if (l < 0) { l = len1 + l; if (l < 0) l = 0; }
    if (h > len1) h = len1;
    if (h < 0) { h = len1 + h; if (h < 0) h = 0; }
    if (l > h) h = l;
    s_del = h - l;
    s_keep = len1 - s_del;
    p1 = _CG_LIST_HDR_PTR(l1);
    x = GC_MALLOC(sz * (size_t)s_keep);
    _CG_LIST_HDR_LEN(l1) = (unsigned int)s_keep;
    _CG_LIST_HDR_TOTAL(l1) = (unsigned int)s_keep;
    _CG_LIST_HDR_PTR(l1) = x;
    p = (char *)x;
    if (l) { memcpy(p, p1, (size_t)l * sz); p += (size_t)l * sz; }
    sh = len1 - h;
    if (sh > 0) memcpy(p, ((char *)p1) + (size_t)h * sz, (size_t)sh * sz);
    return l1;
  }
  /* Identical to _CG_list_getslice's normalisation. */
  if (l == INT32_MIN) {
    l = st < 0 ? len1 - 1 : 0;
  } else if (l < 0) {
    l += len1;
    if (l < 0) l = st < 0 ? -1 : 0;
  } else if (l >= len1) {
    l = st < 0 ? len1 - 1 : len1;
  }
  if (h == INT32_MAX) {
    h = st < 0 ? -1 : len1;
  } else if (h < 0) {
    h += len1;
    if (h < 0) h = st < 0 ? -1 : 0;
  } else if (h >= len1) {
    h = st < 0 ? len1 - 1 : len1;
  }
  if (st > 0)
    n = l < h ? (h - l + st - 1) / st : 0;
  else
    n = l > h ? (l - h + (-st) - 1) / (-st) : 0;
  if (n <= 0) return l1;
  /* A negative step selects the same SET of indices as its positive
   * mirror, and deletion does not care about order -- walk ascending. */
  start = st > 0 ? l : l + (n - 1) * st;
  step = st > 0 ? st : -st;
  p = (char *)_CG_LIST_HDR_PTR(l1);
  w = 0; k = 0; next = start;
  for (r = 0; r < len1; r++) {
    if (k < n && r == next) { k++; next = start + k * step; continue; }
    if (w != r) memcpy(p + (size_t)w * sz, p + (size_t)r * sz, sz);
    w++;
  }
  /* Length shrinks; capacity deliberately does not. */
  _CG_LIST_HDR_LEN(l1) = (unsigned int)w;
  return l1;
}

#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* --- Event Loop Implementation --- */
_CG_ReadyTask* _CG_ready_queue_head = NULL;
_CG_ReadyTask* _CG_ready_queue_tail = NULL;
_CG_TimerTask* _CG_timer_queue_head = NULL;
_CG_IoTask* _CG_io_queue_head = NULL;
_CG_IoTask* _CG_io_queue_tail = NULL;

#include <arpa/inet.h>
int _CG_net_connect(int fd, const char* host, int port) {
  struct hostent *he;
  struct sockaddr_in server;
  if ((he = gethostbyname(host)) == NULL) return -1;
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr = *((struct in_addr *)he->h_addr);
#ifdef __APPLE__
  server.sin_len = sizeof(server);
#endif
  printf("_CG_net_connect host='%s' IP='%s' port=%d\n", host, inet_ntoa(server.sin_addr), port);

  
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  
  int res = connect(fd, (struct sockaddr *)&server, sizeof(struct sockaddr));
  printf("_CG_net_connect connect returned %d, errno %d\n", res, errno);
  if (res < 0 && errno != EINPROGRESS) return -1;
  return 0;
}

// socket.recv / socket.send (pyc_lib/socket.py): BYTES, as in CPython, and
// bytes shares str's length-prefixed buffer, so these serve both. recv keeps
// the length it read (an embedded NUL used to truncate it through
// _CG_String's strlen); send writes the object's own length and prints
// nothing (it printed `_CG_net_write_str called with fd=N` to stdout).
char* _CG_net_read_str(int fd, int max_len) {
  char* buf = (char*)GC_MALLOC_ATOMIC(max_len + 1);
  int n = read(fd, buf, max_len);
  if (n < 0) n = 0;
  buf[n] = '\0';
  return _CG_String_n(buf, (size_t)n);
}

int _CG_net_write_str(int fd, const char* str) {
  return (int)write(fd, str, _CG_string_len(str));
}

int _CG_net_socket(int family, int type, int proto) {
  return socket(family, type, proto);
}

int _CG_net_bind(int fd, const char* host, int port) {
  struct sockaddr_in server;
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons((uint16_t)port);
  if (!host || strlen(host) == 0 || strcmp(host, "0.0.0.0") == 0) {
    server.sin_addr.s_addr = INADDR_ANY;
  } else {
    inet_pton(AF_INET, host, &server.sin_addr);
  }
#ifdef __APPLE__
  server.sin_len = sizeof(server);
#endif
  int opt = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  return bind(fd, (struct sockaddr *)&server, sizeof(server));
}

int _CG_net_listen(int fd, int backlog) {
  return listen(fd, backlog);
}

int _CG_net_accept(int fd) {
  struct sockaddr_in client;
  socklen_t len = sizeof(client);
  return accept(fd, (struct sockaddr *)&client, &len);
}

int _CG_net_close(int fd) {
  return close(fd);
}

int _CG_net_poll_read(int fd, int timeout_ms) {
  struct pollfd pfd;
  pfd.fd = fd;
  pfd.events = POLLIN;
  pfd.revents = 0;
  int res = poll(&pfd, 1, timeout_ms);
  return (res > 0 && (pfd.revents & (POLLIN | POLLHUP | POLLERR))) ? 1 : 0;
}

void _CG_event_loop_register_io(void* hdl, int fd, int events) {
  _CG_IoTask* task = (_CG_IoTask*)GC_MALLOC(sizeof(_CG_IoTask));
  task->hdl = hdl;
  task->fd = fd;
  task->events = events;
  task->next = NULL;
  
  if (!_CG_io_queue_head) {
    _CG_io_queue_head = task;
    _CG_io_queue_tail = task;
  } else {
    _CG_io_queue_tail->next = task;
    _CG_io_queue_tail = task;
  }
}

double _CG_get_time(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

#ifndef __cplusplus
void _CG_resume_coro(void* hdl) {
  void (**vtable)(void*) = (void (**)(void*))hdl;
  if (vtable && vtable[0]) vtable[0](hdl);
}
#endif

void _CG_event_loop_spawn(void* hdl) {
  _CG_ReadyTask* task = (_CG_ReadyTask*)GC_MALLOC(sizeof(_CG_ReadyTask));
  task->hdl = hdl;
  task->next = NULL;
  if (!_CG_ready_queue_head) {
    _CG_ready_queue_head = task;
    _CG_ready_queue_tail = task;
  } else {
    _CG_ready_queue_tail->next = task;
    _CG_ready_queue_tail = task;
  }
}

void _CG_event_loop_sleep(void* hdl, double seconds) {
  _CG_TimerTask* task = (_CG_TimerTask*)GC_MALLOC(sizeof(_CG_TimerTask));
  task->hdl = hdl;
  task->wakeup_time = _CG_get_time() + seconds;
  
  // Insert sorted by wakeup_time
  if (!_CG_timer_queue_head || _CG_timer_queue_head->wakeup_time > task->wakeup_time) {
    task->next = _CG_timer_queue_head;
    _CG_timer_queue_head = task;
  } else {
    _CG_TimerTask* curr = _CG_timer_queue_head;
    while (curr->next && curr->next->wakeup_time <= task->wakeup_time) {
      curr = curr->next;
    }
    task->next = curr->next;
    curr->next = task;
  }
}

void _CG_event_loop_run(void* initial_hdl) {
  if (initial_hdl) {
    _CG_event_loop_spawn(initial_hdl);
  }
  
  while (_CG_ready_queue_head || _CG_timer_queue_head || _CG_io_queue_head) {
    if (_CG_ready_queue_head) {
      _CG_ReadyTask* task = _CG_ready_queue_head;
      _CG_ready_queue_head = task->next;
      if (!_CG_ready_queue_head) _CG_ready_queue_tail = NULL;
      
      void* hdl = task->hdl;
      /* removed free(task) */
      _CG_resume_coro(hdl);
    } else {
      int nfds = 0;
      for (_CG_IoTask* t = _CG_io_queue_head; t; t = t->next) nfds++;
      
      struct pollfd* pfds = NULL;
      if (nfds > 0) {
        pfds = (struct pollfd*)malloc(nfds * sizeof(struct pollfd));
        int i = 0;
        for (_CG_IoTask* t = _CG_io_queue_head; t; t = t->next) {
          pfds[i].fd = t->fd;
          pfds[i].events = t->events;
          i++;
        }
      }
      
      int timeout_ms = -1;
      if (_CG_timer_queue_head) {
        double now = _CG_get_time();
        double wakeup = _CG_timer_queue_head->wakeup_time;
        if (wakeup > now) timeout_ms = (int)((wakeup - now) * 1000.0);
        else timeout_ms = 0;
      }
      
      int n = poll(pfds, nfds, timeout_ms);

      if (n > 0 && nfds > 0) {
        _CG_IoTask* prev = NULL;
        _CG_IoTask* curr = _CG_io_queue_head;
        int j = 0;
        while (curr) {
          if ((pfds[j].revents & curr->events) || (pfds[j].revents & (POLLERR | POLLHUP))) {
            _CG_event_loop_spawn(curr->hdl);
            if (prev) prev->next = curr->next;
            else _CG_io_queue_head = curr->next;
            
            curr = curr->next;
            if (!curr && prev) _CG_io_queue_tail = prev;
            else if (!curr && !prev) _CG_io_queue_tail = NULL;
            /* free(to_free); managed by GC */
          } else {
            prev = curr;
            curr = curr->next;
          }
          j++;
        }
      }
      if (pfds) free(pfds);

      double now = _CG_get_time();
      while (_CG_timer_queue_head && _CG_timer_queue_head->wakeup_time <= now) {
        _CG_TimerTask* task = _CG_timer_queue_head;
        _CG_timer_queue_head = task->next;
        _CG_event_loop_spawn(task->hdl);
        /* task is GC_MALLOC'd, not malloc'd -- issues/022 follow-up:
         * found while wiring up the first real caller of
         * _CG_event_loop_sleep; mirrors the "removed free(task)"
         * convention already used for _CG_ReadyTask/_CG_IoTask just
         * above in this same function -- calling libc free() on
         * Boehm-GC memory is undefined behavior, not a no-op. */
      }
    }
  }
}

/* issues/022 (LLVM backend): an `async def` function's side struct --
 * mirrors _CG_generator_state below, same rationale (no promise
 * reachable from outside the defining coroutine's own IR). Layout
 * MUST match cg_emit_llvm.cc's async_state_struct_type() exactly:
 * {ptr, i64, ptr, i64}. `awaiter` is set by P_prim_await on the
 * coroutine it's about to hand to the event loop; the is_async
 * P_prim_reply epilogue reads it on completion to know who to wake.
 */
typedef struct {
  void *coro_hdl;
  long long value;
  void *awaiter;
  long long done;
} _CG_async_state;

void* _CG_run_coro(void* async_state_ptr) {
  /* issues/022: main()/any top-level async call now returns a
   * pointer to its own async_state (not the raw coroutine handle
   * directly, matching is_generator's identical smuggling shape) --
   * unwrap it before handing the RAW handle to the event loop, which
   * deals in raw handles throughout (_CG_resume_coro's vtable-call
   * convention needs the actual frame pointer, not this wrapper). */
  _CG_async_state *st = (_CG_async_state *)async_state_ptr;
  if (!st) return NULL;
  _CG_event_loop_run(st->coro_hdl);
  return NULL;
}

#ifndef __cplusplus
/* issues/014 (LLVM backend): generator support.
 *
 * A Python generator function's coroutine body is an LLVM coroutine,
 * same family async/await already uses (llvm.coro.* intrinsics,
 * cg_emit_llvm.cc). Unlike the C++20-coroutine C backend
 * (pyc_c_runtime.h's _CG_Generator), where
 * std::coroutine_handle<promise_type>::promise() lets any code
 * holding a handle reach a typed promise object, LLVM's
 * llvm.coro.promise intrinsic only resolves to a real frame offset
 * inside the coroutine's own defining function (where CoroSplit runs)
 * -- it can't be used from here, a separately-compiled, generic C
 * runtime helper with nothing but an opaque handle. So the yielded/
 * sent value, return value, and completion state are tracked in
 * pyc's own small heap struct (cg_emit_llvm.cc's
 * gen_state_struct_type), allocated alongside the raw LLVM coroutine
 * handle; that struct's address -- not the bare coroutine handle --
 * is what flows through Python as __pyc_generator__.handle and what
 * these functions operate on. Layout MUST match
 * gen_state_struct_type()'s {ptr, i64, i64, i64, i64} exactly (all
 * 8-byte-aligned fields, so no padding surprises on either side).
 */
typedef struct {
  void *coro_hdl;
  long long value;
  long long sent;
  long long done;
  long long retval;  /* issues/014: StopIteration.value */
} _CG_generator_state;

/* Resume the raw LLVM coroutine directly via the switched-resume ABI
 * (the frame's first pointer-sized slot is the resume function,
 * called with the handle itself as its one argument) -- the same
 * convention _CG_resume_coro uses for async, reimplemented quietly
 * here (that one prints debug trace on every call, which would
 * corrupt a generator test's captured stdout). */
static void _CG_generator_resume_raw(void *coro_hdl) {
  void (**vtable)(void *) = (void (**)(void *))coro_hdl;
  if (vtable && vtable[0]) vtable[0](coro_hdl);
}

_Bool _CG_generator_advance(long long raw_handle) {
  _CG_generator_state *st = (_CG_generator_state *)(intptr_t)raw_handle;
  if (!st || st->done) return 0;
  st->sent = 0;
  _CG_generator_resume_raw(st->coro_hdl);
  return !st->done;
}

/* .send(v): like advance, but delivers `v` as the value of the
 * paused `x = yield foo` expression being resumed into, instead of
 * None (0). */
_Bool _CG_generator_send(long long raw_handle, long long value) {
  _CG_generator_state *st = (_CG_generator_state *)(intptr_t)raw_handle;
  if (!st || st->done) return 0;
  st->sent = value;
  _CG_generator_resume_raw(st->coro_hdl);
  return !st->done;
}

/* issues/114: identity on the handle, existing only to be opaque to
 * flow analysis -- see pyc_c_runtime.h's inline C-backend twin for why
 * a generator's call result must never be constant folded. */
long long _CG_generator_handle(long long raw_handle) { return raw_handle; }

long long _CG_generator_value(long long raw_handle) {
  _CG_generator_state *st = (_CG_generator_state *)(intptr_t)raw_handle;
  if (!st) return 0;
  return st->value;
}

/* issues/014: a coroutine body with no EXPLICIT `return X` anywhere
 * (bare fall-through, or a bare `return`) still needs *some* int64
 * value flowing into fn->ret purely so FA infers an int return type
 * for the Fun -- cg_emit_llvm.cc's is_generator epilogue now stores
 * whatever's there into gen_state.retval uniformly (see above), so
 * this placeholder's value doubles as that generator's reported
 * StopIteration.value == 0 in the no-explicit-return case (real
 * Python reports None there instead -- a deliberate v1 compromise,
 * same "smuggle through int64" scope as yield/send values). Matches
 * the C backend's identically-named/identically-purposed helper in
 * pyc_c_runtime.h. */
long long _CG_generator_placeholder_return(void) { return 0; }
#endif

/* ---------------------------------------------------------------------
 * hashlib (pyc_lib/hashlib.py): md5, sha1, sha2, sha3/shake, blake2b/s.
 *
 * In C rather than pyc-Python because SHA-512/384, Keccak and BLAKE2b
 * work in unsigned 64-bit words with wraparound, which pyc's int (int64,
 * issues/177) cannot express with CPython semantics; and CPython's own
 * hashlib is C. One context type covers every algorithm. On the Python
 * side it is an immutable bytes value: _CG_hash_update returns an updated
 * COPY, so a bytes object is never mutated in place, and copy() is free.
 * The tables were generated with exact integer arithmetic (cube and square
 * roots of primes, the Keccak LFSR), not typed in.
 * ------------------------------------------------------------------- */

enum {
  _H_MD5, _H_SHA1, _H_SHA224, _H_SHA256, _H_SHA384, _H_SHA512,
  _H_SHA3_224, _H_SHA3_256, _H_SHA3_384, _H_SHA3_512, _H_SHAKE128, _H_SHAKE256,
  _H_BLAKE2B, _H_BLAKE2S, _H_NALG
};

typedef struct {
  int64_t alg, outlen, blocklen, buflen;
  uint64_t cnt_lo, cnt_hi; /* bytes hashed (md/sha); the t counter (blake2) */
  union { uint32_t w32[16]; uint64_t w64[25]; } h;
  uint8_t buf[200];
} _CG_hash_ctx;

static const uint32_t _hs256_K[64] = {
  0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
  0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
  0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
  0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
  0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
  0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
  0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
  0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};
static const uint64_t _hs512_K[80] = {
  0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
  0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
  0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
  0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
  0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
  0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
  0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
  0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
  0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
  0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
  0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
  0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
  0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
  0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
  0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
  0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
  0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
  0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
  0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
  0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL,
};
static const uint32_t _hs256_IV[8] = {
  0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au, 0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
};
static const uint32_t _hs224_IV[8] = {
  0xc1059ed8u, 0x367cd507u, 0x3070dd17u, 0xf70e5939u, 0xffc00b31u, 0x68581511u, 0x64f98fa7u, 0xbefa4fa4u,
};
static const uint64_t _hs512_IV[8] = {
  0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
  0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL,
};
static const uint64_t _hs384_IV[8] = {
  0xcbbb9d5dc1059ed8ULL, 0x629a292a367cd507ULL, 0x9159015a3070dd17ULL, 0x152fecd8f70e5939ULL,
  0x67332667ffc00b31ULL, 0x8eb44a8768581511ULL, 0xdb0c2e0d64f98fa7ULL, 0x47b5481dbefa4fa4ULL,
};
static const uint64_t _hk_RC[24] = {
  0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL, 0x8000000080008000ULL,
  0x000000000000808bULL, 0x0000000080000001ULL, 0x8000000080008081ULL, 0x8000000000008009ULL,
  0x000000000000008aULL, 0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
  0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL, 0x8000000000008003ULL,
  0x8000000000008002ULL, 0x8000000000000080ULL, 0x000000000000800aULL, 0x800000008000000aULL,
  0x8000000080008081ULL, 0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL,
};
static const uint8_t _hk_rot[25] = {0, 1, 62, 28, 27, 36, 44, 6, 55, 20, 3, 10, 43, 25, 39, 41, 45, 15, 21, 8, 18, 2, 61, 56, 14};
static const uint32_t _hmd5_K[64] = {
  0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
  0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu, 0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
  0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau, 0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
  0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu, 0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
  0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu, 0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
  0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u, 0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
  0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
  0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u, 0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u,
};

static const uint32_t _hb2s_IV[8] = {
  0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au, 0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
static const uint8_t _hmd5_S[64] = {
  7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
  5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
  4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
  6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
static const uint8_t _hb2_sigma[10][16] = {
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
  {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
  {11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4},
  {7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8},
  {9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13},
  {2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9},
  {12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11},
  {13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10},
  {6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5},
  {10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0}};

static inline uint32_t _h_rol32(uint32_t x, int c) { return (x << c) | (x >> ((32 - c) & 31)); }
static inline uint32_t _h_ror32(uint32_t x, int c) { return (x >> c) | (x << ((32 - c) & 31)); }
static inline uint64_t _h_rol64(uint64_t x, int c) { return (x << c) | (x >> ((64 - c) & 63)); }
static inline uint64_t _h_ror64(uint64_t x, int c) { return (x >> c) | (x << ((64 - c) & 63)); }
static inline uint32_t _h_ld32le(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline uint32_t _h_ld32be(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
static inline uint64_t _h_ld64le(const uint8_t *p) {
  return (uint64_t)_h_ld32le(p) | ((uint64_t)_h_ld32le(p + 4) << 32);
}
static inline uint64_t _h_ld64be(const uint8_t *p) {
  return ((uint64_t)_h_ld32be(p) << 32) | (uint64_t)_h_ld32be(p + 4);
}

static void _h_md5_compress(uint32_t *h, const uint8_t *p) {
  uint32_t m[16], a = h[0], b = h[1], c = h[2], d = h[3];
  for (int i = 0; i < 16; i++) m[i] = _h_ld32le(p + 4 * i);
  for (int i = 0; i < 64; i++) {
    uint32_t f;
    int g;
    if (i < 16) { f = (b & c) | (~b & d); g = i; }
    else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) & 15; }
    else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) & 15; }
    else { f = c ^ (b | ~d); g = (7 * i) & 15; }
    f = f + a + _hmd5_K[i] + m[g];
    a = d; d = c; c = b;
    b = b + _h_rol32(f, _hmd5_S[i]);
  }
  h[0] += a; h[1] += b; h[2] += c; h[3] += d;
}

static void _h_sha1_compress(uint32_t *h, const uint8_t *p) {
  uint32_t w[80], a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
  for (int t = 0; t < 16; t++) w[t] = _h_ld32be(p + 4 * t);
  for (int t = 16; t < 80; t++) w[t] = _h_rol32(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
  for (int t = 0; t < 80; t++) {
    uint32_t f, k;
    if (t < 20) { f = (b & c) | (~b & d); k = 0x5a827999u; }
    else if (t < 40) { f = b ^ c ^ d; k = 0x6ed9eba1u; }
    else if (t < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdcu; }
    else { f = b ^ c ^ d; k = 0xca62c1d6u; }
    uint32_t tmp = _h_rol32(a, 5) + f + e + k + w[t];
    e = d; d = c; c = _h_rol32(b, 30); b = a; a = tmp;
  }
  h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}

static void _h_sha256_compress(uint32_t *h, const uint8_t *p) {
  uint32_t w[64], v[8];
  for (int t = 0; t < 16; t++) w[t] = _h_ld32be(p + 4 * t);
  for (int t = 16; t < 64; t++) {
    uint32_t s0 = _h_ror32(w[t - 15], 7) ^ _h_ror32(w[t - 15], 18) ^ (w[t - 15] >> 3);
    uint32_t s1 = _h_ror32(w[t - 2], 17) ^ _h_ror32(w[t - 2], 19) ^ (w[t - 2] >> 10);
    w[t] = w[t - 16] + s0 + w[t - 7] + s1;
  }
  for (int i = 0; i < 8; i++) v[i] = h[i];
  for (int t = 0; t < 64; t++) {
    uint32_t S1 = _h_ror32(v[4], 6) ^ _h_ror32(v[4], 11) ^ _h_ror32(v[4], 25);
    uint32_t ch = (v[4] & v[5]) ^ (~v[4] & v[6]);
    uint32_t t1 = v[7] + S1 + ch + _hs256_K[t] + w[t];
    uint32_t S0 = _h_ror32(v[0], 2) ^ _h_ror32(v[0], 13) ^ _h_ror32(v[0], 22);
    uint32_t maj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
    v[7] = v[6]; v[6] = v[5]; v[5] = v[4]; v[4] = v[3] + t1;
    v[3] = v[2]; v[2] = v[1]; v[1] = v[0]; v[0] = t1 + S0 + maj;
  }
  for (int i = 0; i < 8; i++) h[i] += v[i];
}

static void _h_sha512_compress(uint64_t *h, const uint8_t *p) {
  uint64_t w[80], v[8];
  for (int t = 0; t < 16; t++) w[t] = _h_ld64be(p + 8 * t);
  for (int t = 16; t < 80; t++) {
    uint64_t s0 = _h_ror64(w[t - 15], 1) ^ _h_ror64(w[t - 15], 8) ^ (w[t - 15] >> 7);
    uint64_t s1 = _h_ror64(w[t - 2], 19) ^ _h_ror64(w[t - 2], 61) ^ (w[t - 2] >> 6);
    w[t] = w[t - 16] + s0 + w[t - 7] + s1;
  }
  for (int i = 0; i < 8; i++) v[i] = h[i];
  for (int t = 0; t < 80; t++) {
    uint64_t S1 = _h_ror64(v[4], 14) ^ _h_ror64(v[4], 18) ^ _h_ror64(v[4], 41);
    uint64_t ch = (v[4] & v[5]) ^ (~v[4] & v[6]);
    uint64_t t1 = v[7] + S1 + ch + _hs512_K[t] + w[t];
    uint64_t S0 = _h_ror64(v[0], 28) ^ _h_ror64(v[0], 34) ^ _h_ror64(v[0], 39);
    uint64_t maj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
    v[7] = v[6]; v[6] = v[5]; v[5] = v[4]; v[4] = v[3] + t1;
    v[3] = v[2]; v[2] = v[1]; v[1] = v[0]; v[0] = t1 + S0 + maj;
  }
  for (int i = 0; i < 8; i++) h[i] += v[i];
}

static void _h_keccakf(uint64_t *a) {
  for (int round = 0; round < 24; round++) {
    uint64_t c[5], b[25];
    for (int x = 0; x < 5; x++) c[x] = a[x] ^ a[x + 5] ^ a[x + 10] ^ a[x + 15] ^ a[x + 20];
    for (int x = 0; x < 5; x++) {
      uint64_t d = c[(x + 4) % 5] ^ _h_rol64(c[(x + 1) % 5], 1);
      for (int y = 0; y < 25; y += 5) a[x + y] ^= d;
    }
    for (int x = 0; x < 5; x++)
      for (int y = 0; y < 5; y++) b[y + 5 * ((2 * x + 3 * y) % 5)] = _h_rol64(a[x + 5 * y], _hk_rot[x + 5 * y]);
    for (int y = 0; y < 25; y += 5)
      for (int x = 0; x < 5; x++) a[x + y] = b[x + y] ^ (~b[(x + 1) % 5 + y] & b[(x + 2) % 5 + y]);
    a[0] ^= _hk_RC[round];
  }
}

static void _h_blake2b_compress(_CG_hash_ctx *ctx, const uint8_t *p, int last) {
  uint64_t m[16], v[16], *h = ctx->h.w64;
  for (int i = 0; i < 16; i++) m[i] = _h_ld64le(p + 8 * i);
  for (int i = 0; i < 8; i++) { v[i] = h[i]; v[i + 8] = _hs512_IV[i]; }
  v[12] ^= ctx->cnt_lo;
  v[13] ^= ctx->cnt_hi;
  if (last) v[14] = ~v[14];
#define _HB2B_G(a, b, c, d, x, y)                         \
  do {                                                    \
    v[a] = v[a] + v[b] + (x); v[d] = _h_ror64(v[d] ^ v[a], 32); \
    v[c] = v[c] + v[d];       v[b] = _h_ror64(v[b] ^ v[c], 24); \
    v[a] = v[a] + v[b] + (y); v[d] = _h_ror64(v[d] ^ v[a], 16); \
    v[c] = v[c] + v[d];       v[b] = _h_ror64(v[b] ^ v[c], 63); \
  } while (0)
  for (int r = 0; r < 12; r++) {
    const uint8_t *s = _hb2_sigma[r % 10];
    _HB2B_G(0, 4, 8, 12, m[s[0]], m[s[1]]);
    _HB2B_G(1, 5, 9, 13, m[s[2]], m[s[3]]);
    _HB2B_G(2, 6, 10, 14, m[s[4]], m[s[5]]);
    _HB2B_G(3, 7, 11, 15, m[s[6]], m[s[7]]);
    _HB2B_G(0, 5, 10, 15, m[s[8]], m[s[9]]);
    _HB2B_G(1, 6, 11, 12, m[s[10]], m[s[11]]);
    _HB2B_G(2, 7, 8, 13, m[s[12]], m[s[13]]);
    _HB2B_G(3, 4, 9, 14, m[s[14]], m[s[15]]);
  }
#undef _HB2B_G
  for (int i = 0; i < 8; i++) h[i] ^= v[i] ^ v[i + 8];
}

static void _h_blake2s_compress(_CG_hash_ctx *ctx, const uint8_t *p, int last) {
  uint32_t m[16], v[16], *h = ctx->h.w32;
  for (int i = 0; i < 16; i++) m[i] = _h_ld32le(p + 4 * i);
  for (int i = 0; i < 8; i++) { v[i] = h[i]; v[i + 8] = _hb2s_IV[i]; }
  v[12] ^= (uint32_t)ctx->cnt_lo;
  v[13] ^= (uint32_t)(ctx->cnt_lo >> 32);
  if (last) v[14] = ~v[14];
#define _HB2S_G(a, b, c, d, x, y)                         \
  do {                                                    \
    v[a] = v[a] + v[b] + (x); v[d] = _h_ror32(v[d] ^ v[a], 16); \
    v[c] = v[c] + v[d];       v[b] = _h_ror32(v[b] ^ v[c], 12); \
    v[a] = v[a] + v[b] + (y); v[d] = _h_ror32(v[d] ^ v[a], 8);  \
    v[c] = v[c] + v[d];       v[b] = _h_ror32(v[b] ^ v[c], 7);  \
  } while (0)
  for (int r = 0; r < 10; r++) {
    const uint8_t *s = _hb2_sigma[r];
    _HB2S_G(0, 4, 8, 12, m[s[0]], m[s[1]]);
    _HB2S_G(1, 5, 9, 13, m[s[2]], m[s[3]]);
    _HB2S_G(2, 6, 10, 14, m[s[4]], m[s[5]]);
    _HB2S_G(3, 7, 11, 15, m[s[6]], m[s[7]]);
    _HB2S_G(0, 5, 10, 15, m[s[8]], m[s[9]]);
    _HB2S_G(1, 6, 11, 12, m[s[10]], m[s[11]]);
    _HB2S_G(2, 7, 8, 13, m[s[12]], m[s[13]]);
    _HB2S_G(3, 4, 9, 14, m[s[14]], m[s[15]]);
  }
#undef _HB2S_G
  for (int i = 0; i < 8; i++) h[i] ^= v[i] ^ v[i + 8];
}

static int _h_is_sha3(int64_t alg) { return alg >= _H_SHA3_224 && alg <= _H_SHAKE256; }
static int _h_is_blake2(int64_t alg) { return alg == _H_BLAKE2B || alg == _H_BLAKE2S; }

static void _h_compress(_CG_hash_ctx *ctx, const uint8_t *p) {
  switch (ctx->alg) {
    case _H_MD5: _h_md5_compress(ctx->h.w32, p); break;
    case _H_SHA1: _h_sha1_compress(ctx->h.w32, p); break;
    case _H_SHA224: case _H_SHA256: _h_sha256_compress(ctx->h.w32, p); break;
    default: _h_sha512_compress(ctx->h.w64, p); break;
  }
}

/* outlen is the digest size (blake2: the caller's, already validated);
 * key/salt/person are blake2's and already length-checked by the caller. */
static void _h_init(_CG_hash_ctx *ctx, int64_t alg, int64_t outlen, const uint8_t *key, size_t keylen,
                    const uint8_t *salt, size_t saltlen, const uint8_t *person, size_t personlen) {
  static const int64_t outlens[_H_NALG] = {16, 20, 28, 32, 48, 64, 28, 32, 48, 64, 0, 0, 64, 32};
  static const int64_t blocklens[_H_NALG] = {64, 64, 64, 64, 128, 128, 144, 136, 104, 72, 168, 136, 128, 64};
  memset(ctx, 0, sizeof(*ctx));
  ctx->alg = alg;
  ctx->outlen = _h_is_blake2(alg) ? outlen : outlens[alg];
  ctx->blocklen = blocklens[alg];
  switch (alg) {
    case _H_MD5:
    case _H_SHA1: {
      static const uint32_t iv[5] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u, 0xc3d2e1f0u};
      memcpy(ctx->h.w32, iv, sizeof(iv));
      break;
    }
    case _H_SHA224: memcpy(ctx->h.w32, _hs224_IV, sizeof(_hs224_IV)); break;
    case _H_SHA256: memcpy(ctx->h.w32, _hs256_IV, sizeof(_hs256_IV)); break;
    case _H_SHA384: memcpy(ctx->h.w64, _hs384_IV, sizeof(_hs384_IV)); break;
    case _H_SHA512: memcpy(ctx->h.w64, _hs512_IV, sizeof(_hs512_IV)); break;
    case _H_BLAKE2B:
    case _H_BLAKE2S: {
      uint8_t pb[64]; /* the parameter block; fanout = depth = 1 */
      size_t half = alg == _H_BLAKE2B ? 16 : 8, w = alg == _H_BLAKE2B ? 8 : 4;
      memset(pb, 0, sizeof(pb));
      pb[0] = (uint8_t)outlen; pb[1] = (uint8_t)keylen; pb[2] = 1; pb[3] = 1;
      if (saltlen) memcpy(pb + 4 * w, salt, saltlen);
      if (personlen) memcpy(pb + 4 * w + half, person, personlen);
      for (int i = 0; i < 8; i++) {
        if (alg == _H_BLAKE2B) ctx->h.w64[i] = _hs512_IV[i] ^ _h_ld64le(pb + 8 * i);
        else ctx->h.w32[i] = _hb2s_IV[i] ^ _h_ld32le(pb + 4 * i);
      }
      if (keylen) {
        memcpy(ctx->buf, key, keylen);
        ctx->buflen = ctx->blocklen; /* the key block, compressed by the first update or digest */
      }
      break;
    }
    default: break; /* sha3/shake: zero state */
  }
}

static void _h_update(_CG_hash_ctx *ctx, const uint8_t *p, size_t n) {
  int64_t B = ctx->blocklen;
  if (_h_is_sha3(ctx->alg)) {
    for (size_t i = 0; i < n; i++) {
      ctx->h.w64[ctx->buflen >> 3] ^= (uint64_t)p[i] << (8 * (ctx->buflen & 7));
      if (++ctx->buflen == B) { _h_keccakf(ctx->h.w64); ctx->buflen = 0; }
    }
    return;
  }
  if (_h_is_blake2(ctx->alg)) {
    /* the last block must be compressed with the final flag, so a full
     * buffer is held until more input proves it is not the last */
    while (n > 0) {
      if (ctx->buflen == B) {
        ctx->cnt_lo += (uint64_t)B;
        if (ctx->cnt_lo < (uint64_t)B) ctx->cnt_hi++;
        if (ctx->alg == _H_BLAKE2B) _h_blake2b_compress(ctx, ctx->buf, 0);
        else _h_blake2s_compress(ctx, ctx->buf, 0);
        ctx->buflen = 0;
      }
      size_t k = (size_t)(B - ctx->buflen);
      if (k > n) k = n;
      memcpy(ctx->buf + ctx->buflen, p, k);
      ctx->buflen += (int64_t)k; p += k; n -= k;
    }
    return;
  }
  ctx->cnt_lo += (uint64_t)n;
  if (ctx->cnt_lo < (uint64_t)n) ctx->cnt_hi++;
  while (n > 0) {
    if (ctx->buflen == 0 && n >= (size_t)B) { _h_compress(ctx, p); p += B; n -= (size_t)B; continue; }
    size_t k = (size_t)(B - ctx->buflen);
    if (k > n) k = n;
    memcpy(ctx->buf + ctx->buflen, p, k);
    ctx->buflen += (int64_t)k; p += k; n -= k;
    if (ctx->buflen == B) { _h_compress(ctx, ctx->buf); ctx->buflen = 0; }
  }
}

/* Writes n bytes of digest to out; consumes (finalizes) ctx. */
static void _h_final(_CG_hash_ctx *ctx, uint8_t *out, size_t n) {
  int64_t B = ctx->blocklen, alg = ctx->alg;
  if (_h_is_sha3(alg)) {
    uint8_t pad = (alg == _H_SHAKE128 || alg == _H_SHAKE256) ? 0x1f : 0x06;
    ctx->h.w64[ctx->buflen >> 3] ^= (uint64_t)pad << (8 * (ctx->buflen & 7));
    ctx->h.w64[(B - 1) >> 3] ^= (uint64_t)0x80 << (8 * ((B - 1) & 7));
    _h_keccakf(ctx->h.w64);
    for (size_t i = 0, j = 0; i < n; i++, j++) {
      if (j == (size_t)B) { _h_keccakf(ctx->h.w64); j = 0; }
      out[i] = (uint8_t)(ctx->h.w64[j >> 3] >> (8 * (j & 7)));
    }
    return;
  }
  if (_h_is_blake2(alg)) {
    ctx->cnt_lo += (uint64_t)ctx->buflen;
    if (ctx->cnt_lo < (uint64_t)ctx->buflen) ctx->cnt_hi++;
    memset(ctx->buf + ctx->buflen, 0, (size_t)(B - ctx->buflen));
    if (alg == _H_BLAKE2B) {
      _h_blake2b_compress(ctx, ctx->buf, 1);
      for (size_t i = 0; i < n; i++) out[i] = (uint8_t)(ctx->h.w64[i >> 3] >> (8 * (i & 7)));
    } else {
      _h_blake2s_compress(ctx, ctx->buf, 1);
      for (size_t i = 0; i < n; i++) out[i] = (uint8_t)(ctx->h.w32[i >> 2] >> (8 * (i & 3)));
    }
    return;
  }
  /* Merkle-Damgard: 0x80, zeros, then the bit length (64-bit, or 128-bit
   * for the 128-byte-block sha384/512); little-endian for md5 only. */
  int64_t lenbytes = B == 128 ? 16 : 8;
  uint64_t bits_hi = (ctx->cnt_hi << 3) | (ctx->cnt_lo >> 61), bits_lo = ctx->cnt_lo << 3;
  ctx->buf[ctx->buflen++] = 0x80;
  if (ctx->buflen > B - lenbytes) {
    memset(ctx->buf + ctx->buflen, 0, (size_t)(B - ctx->buflen));
    _h_compress(ctx, ctx->buf);
    ctx->buflen = 0;
  }
  memset(ctx->buf + ctx->buflen, 0, (size_t)(B - ctx->buflen));
  for (int i = 0; i < 8; i++) {
    if (alg == _H_MD5) ctx->buf[B - 8 + i] = (uint8_t)(bits_lo >> (8 * i));
    else {
      ctx->buf[B - 1 - i] = (uint8_t)(bits_lo >> (8 * i));
      if (lenbytes == 16) ctx->buf[B - 9 - i] = (uint8_t)(bits_hi >> (8 * i));
    }
  }
  _h_compress(ctx, ctx->buf);
  for (size_t i = 0; i < n; i++) {
    if (alg == _H_MD5) out[i] = (uint8_t)(ctx->h.w32[i >> 2] >> (8 * (i & 3)));
    else if (B == 128) out[i] = (uint8_t)(ctx->h.w64[i >> 3] >> (8 * (7 - (i & 7))));
    else out[i] = (uint8_t)(ctx->h.w32[i >> 2] >> (8 * (3 - (i & 3))));
  }
}

/* libcrypto, loaded at run time (dlopen), never linked: a program pyc
 * builds must not need it to start (the release contract is libc and zstd
 * only, dist_smoke.sh), and it is OpenSSL's architecture-specific code that
 * is wanted (SHA-NI, AVX2), the same digests CPython's hashlib computes.
 * Where it is absent, refuses an algorithm (a FIPS build and md5), or
 * PYC_HASHLIB_BUILTIN is set, the built-in C above is used, as CPython falls
 * back to its own _md5/_sha2. BLAKE2 always uses the built-in code: it takes
 * key/salt/person/digest_size parameters the EVP digest API does not, and
 * OpenSSL is no faster at it. */
typedef struct {
  int tried, ok;
  void *(*ctx_new)(void);
  void (*ctx_free)(void *);
  const void *(*by_name)(const char *);
  const void *(*fetch)(void *, const char *, const char *); /* OpenSSL 3 only */
  int (*init)(void *, const void *, void *);
  int (*update)(void *, const void *, size_t);
  int (*final)(void *, unsigned char *, unsigned int *);
  int (*final_xof)(void *, unsigned char *, size_t);
  int (*copy)(void *, const void *);
  int (*pbkdf2)(const char *, int, const unsigned char *, int, int, const void *, int, unsigned char *);
  const void *md[_H_NALG];
  int md_tried[_H_NALG];
} _h_ossl_t;
static _h_ossl_t _h_ossl;

static int _h_ossl_load(void) {
  if (_h_ossl.tried) return _h_ossl.ok;
  _h_ossl.tried = 1;
  const char *force = getenv("PYC_HASHLIB_BUILTIN");
  if (force && *force && strcmp(force, "0") != 0) return 0;
  static const char *libs[] = {"libcrypto.so.3", "libcrypto.so.1.1"};
  void *h = 0;
  for (size_t i = 0; i < sizeof(libs) / sizeof(libs[0]) && !h; i++) h = dlopen(libs[i], RTLD_NOW | RTLD_LOCAL);
  if (!h) return 0;
#define _H_SYM(field, name) \
  if (!(*(void **)&_h_ossl.field = dlsym(h, name))) return 0
  _H_SYM(ctx_new, "EVP_MD_CTX_new");
  _H_SYM(ctx_free, "EVP_MD_CTX_free");
  _H_SYM(by_name, "EVP_get_digestbyname");
  _H_SYM(init, "EVP_DigestInit_ex");
  _H_SYM(update, "EVP_DigestUpdate");
  _H_SYM(final, "EVP_DigestFinal_ex");
  _H_SYM(final_xof, "EVP_DigestFinalXOF");
  _H_SYM(copy, "EVP_MD_CTX_copy_ex");
  _H_SYM(pbkdf2, "PKCS5_PBKDF2_HMAC");
#undef _H_SYM
  *(void **)&_h_ossl.fetch = dlsym(h, "EVP_MD_fetch");
  return _h_ossl.ok = 1;
}

/* The EVP_MD for an algorithm, or 0 when the built-in code is to be used. */
static const void *_h_ossl_md(int64_t alg) {
  static const char *names[_H_NALG] = {"MD5", "SHA1", "SHA224", "SHA256", "SHA384", "SHA512",
                                       "SHA3-224", "SHA3-256", "SHA3-384", "SHA3-512", "SHAKE128", "SHAKE256",
                                       0, 0};
  if (!names[alg] || !_h_ossl_load()) return 0;
  if (!_h_ossl.md_tried[alg]) {
    _h_ossl.md_tried[alg] = 1;
    /* OpenSSL 3: an explicitly fetched EVP_MD, kept for the process.
     * get_digestbyname's legacy one makes every DigestInit fetch it
     * again, under a lock: 3x CPython's time on small messages. */
    _h_ossl.md[alg] = _h_ossl.fetch ? _h_ossl.fetch(0, names[alg], 0) : _h_ossl.by_name(names[alg]);
  }
  return _h_ossl.md[alg];
}

/* A hash's state: a bytes value the hash object owns and these functions
 * update in place, as the ctypes subset does with _CG_ffi_buf.
 *
 * A libcrypto context is malloc'd and freed by a GC finalizer, and a Boehm
 * finalizer costs about as much as hashing a short message (measured:
 * 4M sha256 objects took 4.5 s with one each, 2.1 s without; CPython, which
 * frees by refcount, 1.5 s). So a hash takes one only once its input
 * passes _H_PREFIX bytes. Until then the input waits in `prefix`, and a
 * digest runs the built-in code over it, which at that size beats setting
 * up a context.
 *
 * mode: _HM_PREFIX  the input so far is in prefix; b is initialized, unfed
 *       _HM_BUILTIN b, the built-in state, is in use
 *       _HM_EVP     evp, libcrypto's EVP_MD_CTX, is in use */
#define _H_PREFIX 256
enum { _HM_PREFIX, _HM_BUILTIN, _HM_EVP };
typedef struct {
  _CG_hash_ctx b;
  void *evp;
  int64_t mode, prefix_len;
  uint8_t prefix[_H_PREFIX];
} _CG_hash_state;

static _CG_hash_state *_h_state(char *s) {
  assert(s && _CG_string_len(s) == sizeof(_CG_hash_state));
  return (_CG_hash_state *)s;
}

static void _h_evp_finalize(void *obj, void *cd) {
  (void)cd;
  _CG_hash_state *st = (_CG_hash_state *)((char *)obj + 8); /* past the bytes length word */
  if (st->evp) _h_ossl.ctx_free(st->evp);
  st->evp = 0;
}

static char *_h_state_alloc(void) {
  char *s = _CG_string_alloc(sizeof(_CG_hash_state));
  memset(s, 0, sizeof(_CG_hash_state));
  return s;
}

static void _h_state_own(char *s, void *evp) {
  _CG_hash_state *st = _h_state(s);
  st->evp = evp;
  st->mode = _HM_EVP;
  GC_register_finalizer(s - 8, _h_evp_finalize, 0, 0, 0);
}

char *_CG_hash_new(_CG_int64 alg, _CG_int64 outlen, char *key, char *salt, char *person) {
  assert(alg >= 0 && alg < _H_NALG);
  char *s = _h_state_alloc();
  _CG_hash_state *st = _h_state(s);
  _h_init(&st->b, alg, outlen, (const uint8_t *)key, _CG_string_len(key), (const uint8_t *)salt,
          _CG_string_len(salt), (const uint8_t *)person, _CG_string_len(person));
  st->mode = _h_ossl_md(alg) ? _HM_PREFIX : _HM_BUILTIN;
  return s;
}

/* Leave _HM_PREFIX: onto libcrypto if it takes this algorithm, else onto
 * the built-in state, carrying the prefix either way. */
static void _h_leave_prefix(char *s) {
  _CG_hash_state *st = _h_state(s);
  const void *md = _h_ossl_md(st->b.alg);
  void *evp = md ? _h_ossl.ctx_new() : 0;
  if (evp && _h_ossl.init(evp, md, 0) == 1 &&
      (!st->prefix_len || _h_ossl.update(evp, st->prefix, (size_t)st->prefix_len) == 1)) {
    _h_state_own(s, evp);
  } else {
    if (evp) _h_ossl.ctx_free(evp);
    if (st->prefix_len) _h_update(&st->b, st->prefix, (size_t)st->prefix_len);
    st->mode = _HM_BUILTIN;
  }
  st->prefix_len = 0;
}

_CG_int64 _CG_hash_update(char *state, char *data) {
  _CG_hash_state *st = _h_state(state);
  size_t n = _CG_string_len(data);
  if (!n) return 0;
  if (st->mode == _HM_PREFIX) {
    if ((size_t)st->prefix_len + n <= _H_PREFIX) {
      memcpy(st->prefix + st->prefix_len, data, n);
      st->prefix_len += (int64_t)n;
      return 0;
    }
    _h_leave_prefix(state);
  }
  if (st->mode == _HM_EVP) {
    if (_h_ossl.update(st->evp, data, n) != 1) {
      fprintf(stderr, "hashlib: libcrypto failed to update a digest\n");
      abort();
    }
  } else {
    _h_update(&st->b, (const uint8_t *)data, n);
  }
  return 0;
}

char *_CG_hash_digest(char *state, _CG_int64 n) {
  _CG_hash_state *st = _h_state(state);
  char *out = _CG_string_alloc((size_t)n);
  if (st->mode == _HM_EVP) {
    /* finalizing consumes a context, so finalize a copy */
    void *tmp = _h_ossl.ctx_new();
    int ok = tmp && _h_ossl.copy(tmp, st->evp) == 1;
    if (ok && (st->b.alg == _H_SHAKE128 || st->b.alg == _H_SHAKE256)) {
      if (n) ok = _h_ossl.final_xof(tmp, (unsigned char *)out, (size_t)n) == 1;
    } else if (ok) {
      unsigned char full[64];
      unsigned int len = 0;
      ok = _h_ossl.final(tmp, full, &len) == 1 && (_CG_int64)len >= n;
      if (ok) memcpy(out, full, (size_t)n);
    }
    if (tmp) _h_ossl.ctx_free(tmp);
    if (!ok) {
      fprintf(stderr, "hashlib: libcrypto failed to finalize a digest\n");
      abort();
    }
    return out;
  }
  _CG_hash_ctx ctx = st->b;
  if (st->mode == _HM_PREFIX && st->prefix_len) _h_update(&ctx, st->prefix, (size_t)st->prefix_len);
  _h_final(&ctx, (uint8_t *)out, (size_t)n);
  return out;
}

char *_CG_hash_copy(char *state) {
  _CG_hash_state *st = _h_state(state);
  char *s = _h_state_alloc();
  memcpy(s, st, sizeof(*st));
  _h_state(s)->evp = 0;
  if (st->mode == _HM_EVP) {
    void *evp = _h_ossl.ctx_new();
    if (!evp || _h_ossl.copy(evp, st->evp) != 1) {
      fprintf(stderr, "hashlib: libcrypto failed to copy a digest context\n");
      abort();
    }
    _h_state_own(s, evp);
  }
  return s;
}

/* Diagnostics: 0 while the input is in the prefix, 1 on the built-in
 * code, 2 on libcrypto. */
_CG_int64 _CG_hash_backend(char *state) { return _h_state(state)->mode; }

/* bytes.hex() and hexdigest() */
char *_CG_bytes_hex(char *b) {
  static const char digits[] = "0123456789abcdef";
  size_t n = _CG_string_len(b);
  char *out = _CG_string_alloc(2 * n);
  for (size_t i = 0; i < n; i++) {
    out[2 * i] = digits[((uint8_t)b[i]) >> 4];
    out[2 * i + 1] = digits[((uint8_t)b[i]) & 15];
  }
  return out;
}

/* PBKDF2 (RFC 8018) over HMAC (RFC 2104) with a fixed-size digest. The
 * inner and outer contexts are keyed once and copied per block. */
char *_CG_hash_pbkdf2(_CG_int64 alg, char *password, char *salt, _CG_int64 iterations, _CG_int64 dklen) {
  size_t plen0 = _CG_string_len(password), slen0 = _CG_string_len(salt);
  const void *md = _h_ossl_md(alg);
  if (md && plen0 <= INT_MAX && slen0 <= INT_MAX && iterations <= INT_MAX && dklen <= INT_MAX) {
    char *out = _CG_string_alloc((size_t)dklen);
    if (_h_ossl.pbkdf2(password ? password : "", (int)plen0, (const unsigned char *)(salt ? salt : ""), (int)slen0,
                       (int)iterations, md, (int)dklen, (unsigned char *)out) == 1)
      return out;
  }
  _CG_hash_ctx ictx, octx, c;
  uint8_t key[200], pad[200], u[64], t[64];
  size_t B, H, plen = _CG_string_len(password), slen = _CG_string_len(salt);
  int64_t outlen = alg == _H_BLAKE2B ? 64 : alg == _H_BLAKE2S ? 32 : 0;
  _h_init(&ictx, alg, outlen, 0, 0, 0, 0, 0, 0);
  B = (size_t)ictx.blocklen;
  H = (size_t)ictx.outlen;
  memset(key, 0, sizeof(key));
  if (plen > B) {
    _h_update(&ictx, (const uint8_t *)password, plen);
    _h_final(&ictx, key, H);
    _h_init(&ictx, alg, outlen, 0, 0, 0, 0, 0, 0);
  } else if (plen) {
    memcpy(key, password, plen);
  }
  octx = ictx;
  for (size_t i = 0; i < B; i++) pad[i] = key[i] ^ 0x36;
  _h_update(&ictx, pad, B);
  for (size_t i = 0; i < B; i++) pad[i] = key[i] ^ 0x5c;
  _h_update(&octx, pad, B);
  char *out = _CG_string_alloc((size_t)dklen);
  for (uint32_t block = 1; (size_t)(block - 1) * H < (size_t)dklen; block++) {
    uint8_t be[4] = {(uint8_t)(block >> 24), (uint8_t)(block >> 16), (uint8_t)(block >> 8), (uint8_t)block};
    c = ictx;
    if (slen) _h_update(&c, (const uint8_t *)salt, slen);
    _h_update(&c, be, 4);
    _h_final(&c, u, H);
    c = octx; _h_update(&c, u, H); _h_final(&c, u, H);
    memcpy(t, u, H);
    for (_CG_int64 j = 1; j < iterations; j++) {
      c = ictx; _h_update(&c, u, H); _h_final(&c, u, H);
      c = octx; _h_update(&c, u, H); _h_final(&c, u, H);
      for (size_t k = 0; k < H; k++) t[k] ^= u[k];
    }
    size_t off = (size_t)(block - 1) * H, k = (size_t)dklen - off < H ? (size_t)dklen - off : H;
    memcpy(out + off, t, k);
  }
  return out;
}

/* ---------------------------------------------------------------------
 * File-descriptor I/O for pyc_lib/os (os.open/read/write/close/pipe/
 * openpty/isatty) and readiness for pyc_lib/select.
 *
 * Each call that can fail returns -1 (or, returning bytes, any value) and
 * leaves the errno in _cg_os_err, 0 on success, so the Python side raises
 * the OSError subclass CPython does. Interrupted calls are retried (PEP
 * 475; pyc has no signal handlers to run in between), except close, which
 * CPython does not retry either. Descriptors pyc creates are
 * non-inheritable (close-on-exec), as CPython's are (PEP 446).
 * ------------------------------------------------------------------- */

/* Declared here because glibc hides them unless _XOPEN_SOURCE (or, for
 * pipe2, _GNU_SOURCE) is set. */
#ifdef __linux__
extern int pipe2(int fds[2], int flags);
#endif
extern int posix_openpt(int flags);
extern int grantpt(int fd);
extern int unlockpt(int fd);
extern char *ptsname(int fd);

/* Constants whose values the platform decides: O_* flags, errnos and
 * select's FD_SETSIZE. The index is the table in pyc_lib/os and select. */
_CG_int64 _CG_os_const(_CG_int64 which) {
  static const _CG_int64 consts[] = {
    O_RDONLY, O_WRONLY, O_RDWR, O_APPEND, O_CREAT, O_EXCL, O_TRUNC, O_NONBLOCK,
    O_NOCTTY, O_CLOEXEC, O_SYNC, O_DSYNC, O_DIRECTORY, O_NOFOLLOW, O_ACCMODE, O_NDELAY,
    EAGAIN, EWOULDBLOCK, EINPROGRESS, EALREADY, EBADF, EINVAL, FD_SETSIZE,
#ifdef __linux__
    1, /* 23: the platform is Linux (pyc_lib/serial picks Linux's ioctls) */
#else
    0,
#endif
    ENOTTY,
  };
  if (which < 0 || which >= (_CG_int64)(sizeof(consts) / sizeof(consts[0]))) abort();
  return consts[which];
}

static int _cg_set_cloexec(int fd) {
  int f = fcntl(fd, F_GETFD);
  return f < 0 ? -1 : fcntl(fd, F_SETFD, f | FD_CLOEXEC);
}

_CG_int64 _CG_os_open(char *path, _CG_int64 flags, _CG_int64 mode) {
  int fd;
  do fd = open(path, (int)flags | O_CLOEXEC, (mode_t)mode);
  while (fd < 0 && errno == EINTR);
  _cg_os_err = fd < 0 ? errno : 0;
  return fd;
}

char *_CG_os_read(_CG_int64 fd, _CG_int64 n) {
  if (n < 0) {
    _cg_os_err = EINVAL;
    return _CG_string_alloc(0);
  }
  char *buf = _CG_string_alloc((size_t)n);
  ssize_t r;
  do r = read((int)fd, buf, (size_t)n);
  while (r < 0 && errno == EINTR);
  if (r < 0) {
    _cg_os_err = errno;
    return _CG_string_alloc(0);
  }
  _cg_os_err = 0;
  if ((_CG_int64)r < n) { /* a short read: return just what was read */
    buf[r] = 0;
    _CG_string_set_len(buf, r);
  }
  return buf;
}

_CG_int64 _CG_os_write(_CG_int64 fd, char *data) {
  size_t n = _CG_string_len(data);
  ssize_t r;
  do r = write((int)fd, n ? data : "", n);
  while (r < 0 && errno == EINTR);
  _cg_os_err = r < 0 ? errno : 0;
  return (_CG_int64)r;
}

_CG_int64 _CG_os_close(_CG_int64 fd) {
  int r = close((int)fd);
  _cg_os_err = r < 0 ? errno : 0;
  return r;
}

_CG_int64 _CG_os_isatty(_CG_int64 fd) { return isatty((int)fd) == 1; }

/* os.ttyname: the terminal's path, or "" with the errno in _cg_os_err. */
char *_CG_os_ttyname(_CG_int64 fd) {
  char buf[4096];
  int e = ttyname_r((int)fd, buf, sizeof(buf));
  _cg_os_err = e;
  return _CG_String(e ? "" : buf);
}

/* A pair of descriptors packed as first | second << 32, or -1. */
_CG_int64 _CG_os_pipe(void) {
  int p[2];
#ifdef __linux__
  int r = pipe2(p, O_CLOEXEC);
#else
  int r = pipe(p);
  if (r == 0 && (_cg_set_cloexec(p[0]) < 0 || _cg_set_cloexec(p[1]) < 0)) {
    int e = errno;
    close(p[0]);
    close(p[1]);
    errno = e;
    r = -1;
  }
#endif
  _cg_os_err = r < 0 ? errno : 0;
  return r < 0 ? -1 : ((_CG_int64)p[0] | ((_CG_int64)p[1] << 32));
}

/* openpty(3) without libutil (a separate library on older glibc and in
 * <util.h> on macOS): CPython's own fallback, posix_openpt + ptsname. */
_CG_int64 _CG_os_openpty(void) {
  int master = posix_openpt(O_RDWR | O_NOCTTY), slave = -1;
  if (master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0 && _cg_set_cloexec(master) == 0) {
    char *name = ptsname(master);
    if (name) {
      do slave = open(name, O_RDWR | O_NOCTTY | O_CLOEXEC);
      while (slave < 0 && errno == EINTR);
    }
  }
  if (slave < 0) {
    int e = errno;
    if (master >= 0) close(master);
    _cg_os_err = e ? e : EIO;
    return -1;
  }
  _cg_os_err = 0;
  return (_CG_int64)master | ((_CG_int64)slave << 32);
}

/* select.select over poll(2). A poll set is a bytes value of n pollfd
 * entries, one per (list, element): the Python side fills it with
 * _CG_poll_set and reads it back with _CG_poll_ready. kind is 1 for the
 * read list, 2 write, 4 exceptional; readiness is what select(2) reports
 * (Linux's POLLIN_SET/POLLOUT_SET/POLLEX_SET), so a hung-up or errored
 * descriptor is readable, as select says it is. */
char *_CG_poll_new(_CG_int64 n) {
  char *b = _CG_string_alloc((size_t)n * sizeof(struct pollfd));
  memset(b, 0, (size_t)n * sizeof(struct pollfd));
  return b;
}

_CG_int64 _CG_poll_set(char *b, _CG_int64 i, _CG_int64 fd, _CG_int64 kind) {
  struct pollfd *p = (struct pollfd *)b + i;
  p->fd = (int)fd;
  p->events = kind == 1 ? POLLIN : kind == 2 ? POLLOUT : POLLPRI;
  p->revents = 0;
  return 0;
}

static double _cg_monotonic(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* timeout in seconds, negative to wait indefinitely. Returns the number of
 * ready entries, or -1 with the errno in _cg_os_err; a descriptor that is
 * not open is EBADF, as select(2) reports it. */
_CG_int64 _CG_poll_wait(char *b, double timeout) {
  nfds_t n = (nfds_t)(_CG_string_len(b) / sizeof(struct pollfd));
  struct pollfd *p = (struct pollfd *)b;
  double deadline = timeout >= 0 ? _cg_monotonic() + timeout : 0;
  for (;;) {
    int ms = -1;
    if (timeout >= 0) {
      double left = deadline - _cg_monotonic();
      /* rounded up, as CPython rounds a select timeout (never early) */
      ms = left <= 0 ? 0 : left >= (double)INT_MAX / 1000 ? INT_MAX : (int)ceil(left * 1000);
    }
    int r = poll(p, n, ms);
    if (r < 0 && errno == EINTR) continue;
    if (r < 0) {
      _cg_os_err = errno;
      return -1;
    }
    for (nfds_t i = 0; i < n; i++)
      if (p[i].revents & POLLNVAL) {
        _cg_os_err = EBADF;
        return -1;
      }
    _cg_os_err = 0;
    return r;
  }
}

_CG_int64 _CG_poll_ready(char *b, _CG_int64 i) {
  struct pollfd *p = (struct pollfd *)b + i;
  short r = p->revents;
  if (p->events == POLLIN) return (r & (POLLIN | POLLHUP | POLLERR)) != 0;
  if (p->events == POLLOUT) return (r & (POLLOUT | POLLERR)) != 0;
  return (r & POLLPRI) != 0;
}

/* os.get_blocking: 1 blocking, 0 not, -1 on error. */
_CG_int64 _CG_os_get_blocking(_CG_int64 fd) {
  int f = fcntl((int)fd, F_GETFL);
  _cg_os_err = f < 0 ? errno : 0;
  return f < 0 ? -1 : !(f & O_NONBLOCK);
}

_CG_int64 _CG_os_set_blocking(_CG_int64 fd, _CG_int64 blocking) {
  int f = fcntl((int)fd, F_GETFL);
  if (f >= 0) f = fcntl((int)fd, F_SETFL, blocking ? (f & ~O_NONBLOCK) : (f | O_NONBLOCK));
  _cg_os_err = f < 0 ? errno : 0;
  return f < 0 ? -1 : 0;
}

/* ---------------------------------------------------------------------
 * termios and fcntl (pyc_lib/termios.py, pyc_lib/fcntl.py). Same error
 * convention as the descriptor calls above: -1 (or any value, returning
 * bytes) with the errno in _cg_os_err. A struct termios travels to Python
 * as a bytes value of its own size, so its layout is the platform's, never
 * restated in Python.
 * ------------------------------------------------------------------- */
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/file.h>

/* termios constants, in pyc_lib/termios.py's order (generated together). */
static const _CG_int64 _cg_termios_consts[] = {
  /* tcsetattr when */
  (_CG_int64)TCSANOW,
  (_CG_int64)TCSADRAIN,
  (_CG_int64)TCSAFLUSH,
  /* tcflush queue */
  (_CG_int64)TCIFLUSH,
  (_CG_int64)TCOFLUSH,
  (_CG_int64)TCIOFLUSH,
  /* tcflow action */
  (_CG_int64)TCOOFF,
  (_CG_int64)TCOON,
  (_CG_int64)TCIOFF,
  (_CG_int64)TCION,
  /* baud rates */
  (_CG_int64)B0,
  (_CG_int64)B50,
  (_CG_int64)B75,
  (_CG_int64)B110,
  (_CG_int64)B134,
  (_CG_int64)B150,
  (_CG_int64)B200,
  (_CG_int64)B300,
  (_CG_int64)B600,
  (_CG_int64)B1200,
  (_CG_int64)B1800,
  (_CG_int64)B2400,
  (_CG_int64)B4800,
  (_CG_int64)B9600,
  (_CG_int64)B19200,
  (_CG_int64)B38400,
  (_CG_int64)B57600,
  (_CG_int64)B115200,
  (_CG_int64)B230400,
  /* iflag */
  (_CG_int64)IGNBRK,
  (_CG_int64)BRKINT,
  (_CG_int64)IGNPAR,
  (_CG_int64)PARMRK,
  (_CG_int64)INPCK,
  (_CG_int64)ISTRIP,
  (_CG_int64)INLCR,
  (_CG_int64)IGNCR,
  (_CG_int64)ICRNL,
  (_CG_int64)IXON,
  (_CG_int64)IXANY,
  (_CG_int64)IXOFF,
  (_CG_int64)IMAXBEL,
  /* oflag */
  (_CG_int64)OPOST,
  (_CG_int64)ONLCR,
  (_CG_int64)OCRNL,
  (_CG_int64)ONOCR,
  (_CG_int64)ONLRET,
  /* cflag */
  (_CG_int64)CSIZE,
  (_CG_int64)CS5,
  (_CG_int64)CS6,
  (_CG_int64)CS7,
  (_CG_int64)CS8,
  (_CG_int64)CSTOPB,
  (_CG_int64)CREAD,
  (_CG_int64)PARENB,
  (_CG_int64)PARODD,
  (_CG_int64)HUPCL,
  (_CG_int64)CLOCAL,
  (_CG_int64)CRTSCTS,
  /* lflag */
  (_CG_int64)ISIG,
  (_CG_int64)ICANON,
  (_CG_int64)ECHO,
  (_CG_int64)ECHOE,
  (_CG_int64)ECHOK,
  (_CG_int64)ECHONL,
  (_CG_int64)NOFLSH,
  (_CG_int64)TOSTOP,
  (_CG_int64)IEXTEN,
  (_CG_int64)ECHOCTL,
  (_CG_int64)ECHOKE,
  (_CG_int64)ECHOPRT,
  (_CG_int64)FLUSHO,
  (_CG_int64)PENDIN,
  /* cc indices */
  (_CG_int64)NCCS,
  (_CG_int64)VEOF,
  (_CG_int64)VEOL,
  (_CG_int64)VEOL2,
  (_CG_int64)VERASE,
  (_CG_int64)VWERASE,
  (_CG_int64)VINTR,
  (_CG_int64)VKILL,
  (_CG_int64)VQUIT,
  (_CG_int64)VSTART,
  (_CG_int64)VSTOP,
  (_CG_int64)VSUSP,
  (_CG_int64)VREPRINT,
  (_CG_int64)VLNEXT,
  (_CG_int64)VDISCARD,
  (_CG_int64)VMIN,
  (_CG_int64)VTIME,
  /* ioctl requests and modem bits */
  (_CG_int64)TIOCMGET,
  (_CG_int64)TIOCMSET,
  (_CG_int64)TIOCMBIS,
  (_CG_int64)TIOCMBIC,
  (_CG_int64)TIOCM_LE,
  (_CG_int64)TIOCM_DTR,
  (_CG_int64)TIOCM_RTS,
  (_CG_int64)TIOCM_ST,
  (_CG_int64)TIOCM_SR,
  (_CG_int64)TIOCM_CTS,
  (_CG_int64)TIOCM_CAR,
  (_CG_int64)TIOCM_CD,
  (_CG_int64)TIOCM_RNG,
  (_CG_int64)TIOCM_RI,
  (_CG_int64)TIOCM_DSR,
  (_CG_int64)FIONREAD,
  (_CG_int64)TIOCOUTQ,
  (_CG_int64)TIOCEXCL,
  (_CG_int64)TIOCNXCL,
  (_CG_int64)TIOCGWINSZ,
  (_CG_int64)TIOCSWINSZ,
};

_CG_int64 _CG_termios_const(_CG_int64 which) {
  if (which < 0 || which >= (_CG_int64)(sizeof(_cg_termios_consts) / sizeof(_cg_termios_consts[0]))) abort();
  return _cg_termios_consts[which];
}

char *_CG_tc_get(_CG_int64 fd) {
  char *t = _CG_string_alloc(sizeof(struct termios));
  _cg_os_err = tcgetattr((int)fd, (struct termios *)t) < 0 ? errno : 0;
  return t;
}

/* 0..3 the iflag, oflag, cflag and lflag words; 4, 5 the input and output
 * speeds (a B* constant, as CPython reports them). */
_CG_int64 _CG_tc_field(char *t, _CG_int64 which) {
  struct termios *m = (struct termios *)t;
  switch (which) {
    case 0: return (_CG_int64)m->c_iflag;
    case 1: return (_CG_int64)m->c_oflag;
    case 2: return (_CG_int64)m->c_cflag;
    case 3: return (_CG_int64)m->c_lflag;
    case 4: return (_CG_int64)cfgetispeed(m);
    default: return (_CG_int64)cfgetospeed(m);
  }
}

_CG_int64 _CG_tc_cc(char *t, _CG_int64 i) { return (_CG_int64)((struct termios *)t)->c_cc[i]; }

/* CPython's tcsetattr: start from the descriptor's current settings, so
 * fields the list does not carry keep their values. cc is NCCS bytes. */
_CG_int64 _CG_tc_set(_CG_int64 fd, _CG_int64 when, _CG_int64 iflag, _CG_int64 oflag, _CG_int64 cflag,
                     _CG_int64 lflag, _CG_int64 ispeed, _CG_int64 ospeed, char *cc) {
  struct termios m;
  if (tcgetattr((int)fd, &m) < 0) goto fail;
  m.c_iflag = (tcflag_t)iflag;
  m.c_oflag = (tcflag_t)oflag;
  m.c_cflag = (tcflag_t)cflag;
  m.c_lflag = (tcflag_t)lflag;
  if (cfsetispeed(&m, (speed_t)ispeed) < 0 || cfsetospeed(&m, (speed_t)ospeed) < 0) goto fail;
  memcpy(m.c_cc, cc, NCCS);
  if (tcsetattr((int)fd, (int)when, &m) < 0) goto fail;
  _cg_os_err = 0;
  return 0;
fail:
  _cg_os_err = errno;
  return -1;
}

static _CG_int64 _cg_rc(int r) {
  _cg_os_err = r < 0 ? errno : 0;
  return r < 0 ? -1 : r;
}

_CG_int64 _CG_tc_sendbreak(_CG_int64 fd, _CG_int64 duration) { return _cg_rc(tcsendbreak((int)fd, (int)duration)); }
_CG_int64 _CG_tc_drain(_CG_int64 fd) { return _cg_rc(tcdrain((int)fd)); }
_CG_int64 _CG_tc_flush(_CG_int64 fd, _CG_int64 queue) { return _cg_rc(tcflush((int)fd, (int)queue)); }
_CG_int64 _CG_tc_flow(_CG_int64 fd, _CG_int64 action) { return _cg_rc(tcflow((int)fd, (int)action)); }

/* rows | cols << 32, or -1 */
_CG_int64 _CG_tc_getwinsize(_CG_int64 fd) {
  struct winsize w;
  if (_cg_rc(ioctl((int)fd, TIOCGWINSZ, &w)) < 0) return -1;
  return (_CG_int64)w.ws_row | ((_CG_int64)w.ws_col << 32);
}

/* CPython's: the pixel sizes are kept from the current setting. */
_CG_int64 _CG_tc_setwinsize(_CG_int64 fd, _CG_int64 rows, _CG_int64 cols) {
  struct winsize w;
  if (_cg_rc(ioctl((int)fd, TIOCGWINSZ, &w)) < 0) return -1;
  w.ws_row = (unsigned short)rows;
  w.ws_col = (unsigned short)cols;
  return _cg_rc(ioctl((int)fd, TIOCSWINSZ, &w));
}

/* fcntl constants, in pyc_lib/fcntl.py's order. */
_CG_int64 _CG_fcntl_const(_CG_int64 which) {
  static const _CG_int64 consts[] = {
    F_DUPFD, F_DUPFD_CLOEXEC, F_GETFD, F_SETFD, F_GETFL, F_SETFL, FD_CLOEXEC,
    LOCK_SH, LOCK_EX, LOCK_NB, LOCK_UN,
  };
  if (which < 0 || which >= (_CG_int64)(sizeof(consts) / sizeof(consts[0]))) abort();
  return consts[which];
}

_CG_int64 _CG_fcntl_int(_CG_int64 fd, _CG_int64 cmd, _CG_int64 arg) {
  int r;
  do r = fcntl((int)fd, (int)cmd, (int)arg);
  while (r < 0 && errno == EINTR);
  return _cg_rc(r);
}

/* CPython's buffer forms: the argument is copied into a 1024-byte buffer
 * (the caller has checked its length), the call may write into it, and the
 * result is a bytes of the argument's length. */
static char *_cg_buf_result(int r, char *buf, size_t n) {
  if (r < 0) {
    _cg_os_err = errno;
    return _CG_string_alloc(0);
  }
  _cg_os_err = 0;
  char *out = _CG_string_alloc(n);
  memcpy(out, buf, n);
  return out;
}

char *_CG_fcntl_buf(_CG_int64 fd, _CG_int64 cmd, char *arg) {
  char buf[1024];
  size_t n = _CG_string_len(arg);
  memset(buf, 0, sizeof(buf));
  if (n) memcpy(buf, arg, n);
  int r;
  do r = fcntl((int)fd, (int)cmd, buf);
  while (r < 0 && errno == EINTR);
  return _cg_buf_result(r, buf, n);
}

_CG_int64 _CG_ioctl_int(_CG_int64 fd, _CG_int64 request, _CG_int64 arg) {
  return _cg_rc(ioctl((int)fd, (unsigned long)request, (int)arg));
}

char *_CG_ioctl_buf(_CG_int64 fd, _CG_int64 request, char *arg) {
  char buf[1024];
  size_t n = _CG_string_len(arg);
  memset(buf, 0, sizeof(buf));
  if (n) memcpy(buf, arg, n);
  return _cg_buf_result(ioctl((int)fd, (unsigned long)request, buf), buf, n);
}

_CG_int64 _CG_flock(_CG_int64 fd, _CG_int64 op) {
  int r;
  do r = flock((int)fd, (int)op);
  while (r < 0 && errno == EINTR);
  return _cg_rc(r);
}
