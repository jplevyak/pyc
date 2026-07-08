#pragma once

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "gc.h"
#include <sys/time.h>
#include <unistd.h>
#include <sys/socket.h>

#ifdef __cplusplus
#include <coroutine>
#endif

#ifdef __cplusplus
extern "C" {
#endif

void __pyc_net_wait_write__(int fd);
void __pyc_net_wait_read__(int fd);
int _CG_net_connect(int fd, const char* host, int port);
char* _CG_net_read_str(int fd, int size);
int _CG_net_write_str(int fd, const char* data);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Event Loop Queues
typedef struct _CG_ReadyTask {
  void* hdl;
  struct _CG_ReadyTask* next;
} _CG_ReadyTask;

typedef struct _CG_TimerTask {
  void* hdl;
  double wakeup_time;
  struct _CG_TimerTask* next;
} _CG_TimerTask;

typedef struct _CG_IoTask {
  void* hdl;
  int fd;
  int events;
  struct _CG_IoTask* next;
} _CG_IoTask;

extern _CG_ReadyTask* _CG_ready_queue_head;
extern _CG_ReadyTask* _CG_ready_queue_tail;
extern _CG_TimerTask* _CG_timer_queue_head;
extern _CG_IoTask* _CG_io_queue_head;
extern _CG_IoTask* _CG_io_queue_tail;

double _CG_get_time(void);
void _CG_resume_coro(void* hdl);
void _CG_event_loop_spawn(void* hdl);
void _CG_event_loop_sleep(void* hdl, double seconds);
void _CG_event_loop_register_io(void* hdl, int fd, int events);
void _CG_event_loop_run(void* initial_hdl);
void* _CG_run_coro(void* coro_hdl);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <coroutine>

struct _CG_Coroutine {
  struct promise_type {
    void* value = nullptr;
    std::coroutine_handle<> awaiter;

    _CG_Coroutine get_return_object() {
      return _CG_Coroutine{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    std::suspend_always initial_suspend() { return {}; }
    
    struct final_awaiter {
      bool await_ready() const noexcept { return false; }
      std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
        if (h.promise().awaiter) return h.promise().awaiter;
        return std::noop_coroutine();
      }
      void await_resume() noexcept {}
    };
    final_awaiter final_suspend() noexcept { return {}; }

    template<typename T>
    void return_value(T v) { value = (void*)(uintptr_t)v; }
    void unhandled_exception() {}
  };

  std::coroutine_handle<promise_type> handle;

  bool await_ready() const noexcept { return handle.done(); }
  std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting_handle) noexcept {
    handle.promise().awaiter = awaiting_handle;
    return handle; // resume this coroutine immediately
  }
  void* await_resume() const noexcept { return handle.promise().value; }
};

inline void* _CG_run_coro(_CG_Coroutine coro) {
  _CG_event_loop_run(coro.handle.address());
  return coro.handle.promise().value;
}

struct _CG_Await_Net_Read {
  int fd;
  bool await_ready() const noexcept { return false; }
  void await_suspend(std::coroutine_handle<> h) noexcept {
    _CG_event_loop_register_io(h.address(), fd, 1);
  }
  int await_resume() const noexcept { return 0; }
};

struct _CG_Await_Net_Write {
  int fd;
  bool await_ready() const noexcept { return false; }
  void await_suspend(std::coroutine_handle<> h) noexcept {
    _CG_event_loop_register_io(h.address(), fd, 4);
  }
  int await_resume() const noexcept { return 0; }
};

#endif

/* --- Micro-Core: Memory Interface --- */
#define _CG_Memory_Alloc(sz) GC_MALLOC(sz)
#define _CG_Memory_Realloc(p, sz) GC_REALLOC(p, sz)
#define _CG_Memory_Free(p)
#define _CG_Memory_Init() GC_INIT()
#define _CG_Memory_Copy(dst, src, sz) memcpy((dst), (src), (sz))
#define _CG_Memory_Set(dst, val, sz) memset((dst), (val), (sz))

/* --- Micro-Core: Syscall Interface --- */
#define _CG_Syscall_Write(fd, buf, sz) fwrite((buf), 1, (sz), (fd) == 1 ? stdout : ((fd) == 2 ? stderr : stdout))
#define _CG_Syscall_Exit(code) exit(code)

/* Legacy mappings to Micro-Core */
#define MALLOC _CG_Memory_Alloc
#define REALLOC _CG_Memory_Realloc
#define FREE(_x) _CG_Memory_Free(_x)
#define MEM_INIT() _CG_Memory_Init()

typedef char int8;
typedef unsigned char uint8;
typedef int int32;
typedef unsigned int uint32;
typedef long long int64;
typedef unsigned long long uint64;
typedef short int16;
typedef unsigned short uint16;
#ifdef __APPLE__
typedef uint32 uint;
#endif
typedef float float32;
typedef double float64;
typedef struct {
  float32 r;
  float32 i;
} complex32;
typedef struct {
  float64 r;
  float64 i;
} complex64;

typedef void *_CG_symbol;
typedef void *_CG_function;
typedef void *_CG_tuple;
typedef void *_CG_list;
typedef void *_CG_vector;
typedef void *_CG_continuation;
typedef void *_CG_any;
typedef void *_CG_null;
typedef void *_CG_void;
typedef void *_CG_void_type;
typedef void *_CG_object;
typedef int _CG_int;
typedef uint8 _CG_bool;
typedef uint8 _CG_uint8;
typedef uint16 _CG_uint16;
typedef uint32 _CG_uint32;
typedef uint64 _CG_uint64;
typedef int8 _CG_int8;
typedef int16 _CG_int16;
typedef int32 _CG_int32;
typedef int64 _CG_int64;
typedef float32 _CG_float32;
typedef float64 _CG_float64;
typedef complex32 _CG_complex32;
typedef complex64 _CG_complex64;
typedef char *_CG_string;
typedef void *_CG_ref;
typedef void *_CG_fun;
typedef void *_CG_nil_type;
#define _CG_reply _CG_symbol

/* FFI Wrappers for Python (__pyc_c_call__) */
inline _CG_int64 _CG_FFI_Alloc(_CG_int64 sz) { return (_CG_int64)(uintptr_t)_CG_Memory_Alloc(sz); }
inline void _CG_FFI_Free(_CG_int64 p) { _CG_Memory_Free((void*)(uintptr_t)p); }
inline _CG_int64 _CG_FFI_Get_Int64(_CG_int64 p, _CG_int64 offset) { return *(_CG_int64*)((char*)(uintptr_t)p + offset); }
inline void _CG_FFI_Set_Int64(_CG_int64 p, _CG_int64 offset, _CG_int64 val) { *(_CG_int64*)((char*)(uintptr_t)p + offset) = val; }
inline _CG_int8 _CG_FFI_Get_Int8(_CG_int64 p, _CG_int64 offset) { return *(_CG_int8*)((char*)(uintptr_t)p + offset); }
inline void _CG_FFI_Set_Int8(_CG_int64 p, _CG_int64 offset, _CG_int8 val) { *(_CG_int8*)((char*)(uintptr_t)p + offset) = val; }
#define _CG_primitive _CG_symbol
#define _CG_make_tuple _CG_symbol
#define _CG_Symbol(_x, _y) ((void *)(uintptr_t)_x)
#define null ((void *)0)
#define bool int
#define True 1
#define False 0
#define __init ((void *)0)
#define nil_type 0

/* Type Tags and Objects */
typedef enum {
  PYC_TAG_INT64,
  PYC_TAG_STRING,
  PYC_TAG_FLOAT64,
  PYC_TAG_BOOL,
  PYC_TAG_NIL,
  PYC_TAG_OBJECT,
  PYC_TAG_ANY
} _CG_TypeTag;

typedef struct {
  _CG_TypeTag tag;
  const char *name;
} _CG_TypeObject;

static _CG_TypeObject _CG_type_int64 = { PYC_TAG_INT64, "int64" };
static _CG_TypeObject _CG_type_str = { PYC_TAG_STRING, "str" };
static _CG_TypeObject _CG_type_float64 = { PYC_TAG_FLOAT64, "float64" };
static _CG_TypeObject _CG_type_bool = { PYC_TAG_BOOL, "bool" };
static _CG_TypeObject _CG_type_nil_type = { PYC_TAG_NIL, "nil_type" };
static _CG_TypeObject _CG_type_object = { PYC_TAG_OBJECT, "object" };
static _CG_TypeObject _CG_type_any = { PYC_TAG_ANY, "any" };

#define _CG_prim_isinstance(obj, type_obj) ((type_obj) == &_CG_type_nil_type ? ((void*)(obj) == NULL) : 0)

/*
  Strings

  Strings are pointers to the data portion (C-like) preceeded by
  an 8-byte length.  This makes them compatible with C and still
  permits them to contain \0 and makes obtaining the length O(1).

  Strings have a 0 sentinal at the end for C compatibility.
*/

#define _CG_string_len(_s) ((_s) ? (size_t) * (int64 *)(((char *)(_s)) - 8) : 0)
#define _CG_string_set_len(_s, _v) (*(int64 *)(((char *)(_s)) - 8)) = (int64)(_v)

inline char *_CG_string_alloc(size_t s) {
  char *str = (char *)GC_MALLOC(s + 8 + 1);
  str += 8;
  str[s] = 0;
  _CG_string_set_len(str, s);
  return str;
}

inline char *_CG_String(const void *x) {
  size_t len = strlen((char *)x);
  char *str = _CG_string_alloc(len);
  memcpy(str, x, len);
  return str;
}

inline char *_CG_format_string(char *str, ...) {
  int l = _CG_string_len(str) + 24;
  char *s = 0;
  va_list ap;
  while (1) {
    va_start(ap, str);
    s = _CG_string_alloc(l);
    int ll = vsnprintf(s, l, str, ap);
    if (ll < l - 1) {
      _CG_string_set_len(s, ll);
      break;
    }
    l = l * 2;
  }
  return s;
}

// D.4: typed runtime helper for int.__str__ in the library.
// Replaces the v2 LLVM emit_prim_to_string inline emission for
// integer arguments. Both backends now route through the same
// out-of-line definition (pyc_c_runtime.h gives the C backend
// its static-inline copy; pyc_runtime.c gives the LLVM backend
// a linkable extern).
inline char *_CG_str_from_int(int64 x) {
  char tmp[32];
  int n = snprintf(tmp, sizeof(tmp), "%lld", (long long)x);
  if (n < 0) n = 0;
  if ((size_t)n >= sizeof(tmp)) n = sizeof(tmp) - 1;
  char *s = _CG_string_alloc(n);
  memcpy(s, tmp, n);
  return s;
}

// D.5: float → str with the Python ".0" suffix for whole numbers.
// %.17g preserves round-trip precision but strips trailing zeros;
// CPython's `str(0.0)` is "0.0" and `str(2.0)` is "2.0", so we
// scan the formatted output and append ".0" if it has no decimal
// point or exponent. Mirrors the existing C++-only overload
// _CG_prim_primitive_to_string(double) in this header but with a
// unique C-callable name so libpyc_runtime.a can export it.
inline char *_CG_str_from_float(double d) {
  char tmp[64];
  int n = snprintf(tmp, sizeof(tmp), "%.17g", d);
  if (n < 0) n = 0;
  if ((size_t)n >= sizeof(tmp)) n = sizeof(tmp) - 1;
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

// File I/O helpers for the library-level file object (__pyc__/07_file.py:
// open(), read/readline/write/close, sys.std{in,out,err}, input()).
// Handles are FILE* smuggled through int64 -- the library stores them in
// a plain int field. A failed fopen returns 0; the library treats a zero
// handle as an immediately-EOF/ignore-writes file rather than raising
// (pyc has no exception model yet, issue 011).
inline int64 _CG_fopen(char *path, char *mode) { return (int64)(intptr_t)fopen(path, mode); }
inline int64 _CG_fstd(int64 which) { return (int64)(intptr_t)(which == 0 ? stdin : which == 1 ? stdout : stderr); }
inline int64 _CG_fclose(int64 h) { return h ? (int64)fclose((FILE *)(intptr_t)h) : 0; }
inline int64 _CG_fflush(int64 h) { return h ? (int64)fflush((FILE *)(intptr_t)h) : 0; }
inline int64 _CG_fwrite_str(int64 h, char *s) {
  if (!h) return 0;
  return (int64)fwrite(s, 1, _CG_string_len(s), (FILE *)(intptr_t)h);
}
// Entire rest of the stream; NUL-safe (length-prefixed strings), so
// binary reads ('rb') work too -- pyc has no separate bytes type.
inline char *_CG_fread_all(int64 h) {
  if (!h) return _CG_string_alloc(0);
  FILE *f = (FILE *)(intptr_t)h;
  size_t cap = 4096, len = 0;
  char *buf = (char *)GC_MALLOC(cap);
  size_t r;
  while ((r = fread(buf + len, 1, cap - len, f)) > 0) {
    len += r;
    if (len == cap) {
      char *nb = (char *)GC_MALLOC(cap * 2);
      memcpy(nb, buf, len);
      buf = nb;
      cap *= 2;
    }
  }
  char *s = _CG_string_alloc(len);
  memcpy(s, buf, len);
  return s;
}
inline char *_CG_fread_n(int64 h, int64 n) {
  if (!h || n <= 0) return _CG_string_alloc(0);
  char *s = _CG_string_alloc((size_t)n);
  size_t r = fread(s, 1, (size_t)n, (FILE *)(intptr_t)h);
  if ((int64)r == n) return s;
  char *s2 = _CG_string_alloc(r);
  memcpy(s2, s, r);
  return s2;
}
// One line INCLUDING the trailing '\n' (CPython readline semantics);
// empty string at EOF.
inline char *_CG_freadline(int64 h) {
  if (!h) return _CG_string_alloc(0);
  FILE *f = (FILE *)(intptr_t)h;
  size_t cap = 256, len = 0;
  char *buf = (char *)GC_MALLOC(cap);
  int c;
  while ((c = fgetc(f)) != EOF) {
    if (len == cap) {
      char *nb = (char *)GC_MALLOC(cap * 2);
      memcpy(nb, buf, len);
      buf = nb;
      cap *= 2;
    }
    buf[len++] = (char)c;
    if (c == '\n') break;
  }
  char *s = _CG_string_alloc(len);
  memcpy(s, buf, len);
  return s;
}

// issues/006: PEP 3101 format-spec mini-language for f-strings
// (`f"{x:.2f}"`, `f"{x:>10}"`, `f"{x:,}"`, etc.) and `__format__`.
//
// format_spec ::= [[fill]align][sign]["#"]["0"][width][","|"_"]["." precision][type]
//
// Parsed once per call into `_CG_FormatSpec`; `_CG_format_int_spec`/
// `_CG_format_float_spec`/`_CG_format_str_spec` each build a "core"
// string (sign + digits/text, no width padding) using ordinary
// printf conversions for the numeric cases, then hand off to the
// shared `_CG_group_digits` (comma/underscore grouping) and
// `_CG_pad_align` (width/fill/alignment, including printf-unsupported
// cases like a custom fill character or center alignment) helpers,
// which work uniformly across all three types. `n`/`c` and locale-aware
// grouping are not implemented (treated as `d`/plain width padding);
// dynamic width/precision (`{x:{width}}`) are handled by the frontend
// re-parsing the spec as a literal string, so they're out of scope here.
typedef struct {
  char fill;
  char align;   // '<', '>', '^', '=', or 0 (unspecified)
  char sign;    // '+', '-', ' '
  int alt;      // '#' flag
  int zero;     // '0' flag
  int width;    // -1 if unspecified
  char group;   // ',', '_', or 0
  int precision;  // -1 if unspecified
  char type;    // presentation type, or 0 if unspecified
} _CG_FormatSpec;

inline void _CG_parse_format_spec(const char *spec, _CG_FormatSpec *out) {
  out->fill = ' ';
  out->align = 0;
  out->sign = '-';
  out->alt = 0;
  out->zero = 0;
  out->width = -1;
  out->group = 0;
  out->precision = -1;
  out->type = 0;
  const char *p = spec;
  if (p[0] && p[1] && (p[1] == '<' || p[1] == '>' || p[1] == '^' || p[1] == '=')) {
    out->fill = p[0];
    out->align = p[1];
    p += 2;
  } else if (p[0] == '<' || p[0] == '>' || p[0] == '^' || p[0] == '=') {
    out->align = p[0];
    p += 1;
  }
  if (p[0] == '+' || p[0] == '-' || p[0] == ' ') {
    out->sign = p[0];
    p++;
  }
  if (p[0] == '#') {
    out->alt = 1;
    p++;
  }
  if (p[0] == '0') {
    out->zero = 1;
    p++;
    if (!out->align) {
      out->align = '=';
      out->fill = '0';
    }
  }
  if (p[0] >= '0' && p[0] <= '9') {
    int w = 0;
    while (p[0] >= '0' && p[0] <= '9') {
      w = w * 10 + (*p - '0');
      p++;
    }
    out->width = w;
  }
  if (p[0] == ',' || p[0] == '_') {
    out->group = p[0];
    p++;
  }
  if (p[0] == '.') {
    p++;
    int pr = 0;
    while (p[0] >= '0' && p[0] <= '9') {
      pr = pr * 10 + (*p - '0');
      p++;
    }
    out->precision = pr;
  }
  if (p[0]) out->type = p[0];
}

// Insert `sep` every 3 digits (from the right) in `core`'s integer
// part, skipping a leading sign and stopping at a decimal point.
// Caller frees the result with `free`.
inline char *_CG_group_digits(const char *core, char sep) {
  int len = (int)strlen(core);
  int sign_len = (core[0] == '-' || core[0] == '+' || core[0] == ' ') ? 1 : 0;
  int dot = len;
  for (int i = sign_len; i < len; i++)
    if (core[i] == '.') {
      dot = i;
      break;
    }
  int int_len = dot - sign_len;
  int groups = int_len > 0 ? (int_len - 1) / 3 : 0;
  char *out = (char *)malloc(len + groups + 1);
  int oi = 0;
  for (int i = 0; i < sign_len; i++) out[oi++] = core[i];
  for (int i = sign_len; i < dot; i++) {
    int from_left = i - sign_len;
    int remaining_after = int_len - from_left - 1;
    out[oi++] = core[i];
    if (remaining_after > 0 && remaining_after % 3 == 0) out[oi++] = sep;
  }
  for (int i = dot; i < len; i++) out[oi++] = core[i];
  out[oi] = 0;
  return out;
}

// Pad/align `core` to `width` using `fill`. `sign_len` is the number
// of leading sign/space characters in `core` (only meaningful for
// '=' alignment, which pads between the sign and the digits).
// Returns a freshly-allocated pyc string.
inline char *_CG_pad_align(const char *core, int width, char align, char fill, int sign_len) {
  int len = (int)strlen(core);
  if (width <= len) return _CG_String(core);
  int pad = width - len;
  char *out = (char *)malloc((size_t)width + 1);
  int oi = 0;
  if (align == '>') {
    for (int i = 0; i < pad; i++) out[oi++] = fill;
    memcpy(out + oi, core, len);
    oi += len;
  } else if (align == '^') {
    int left = pad / 2, right = pad - left;
    for (int i = 0; i < left; i++) out[oi++] = fill;
    memcpy(out + oi, core, len);
    oi += len;
    for (int i = 0; i < right; i++) out[oi++] = fill;
  } else if (align == '=') {
    memcpy(out + oi, core, sign_len);
    oi += sign_len;
    for (int i = 0; i < pad; i++) out[oi++] = fill;
    memcpy(out + oi, core + sign_len, len - sign_len);
    oi += len - sign_len;
  } else {
    memcpy(out + oi, core, len);
    oi += len;
    for (int i = 0; i < pad; i++) out[oi++] = fill;
  }
  out[oi] = 0;
  char *r = _CG_String(out);
  free(out);
  return r;
}

inline char *_CG_format_float_spec(double val, const char *spec_str) {
  _CG_FormatSpec fs;
  _CG_parse_format_spec(spec_str, &fs);
  int prec = fs.precision >= 0 ? fs.precision : 6;
  int isneg = val < 0 || (val == 0 && signbit(val));
  double av = isneg ? -val : val;
  const char *sign_str = isneg ? "-" : (fs.sign == '+' ? "+" : (fs.sign == ' ' ? " " : ""));
  char buf[512];
  switch (fs.type) {
    case 'f':
    case 'F':
      snprintf(buf, sizeof(buf), fs.alt ? "%#.*f" : "%.*f", prec, av);
      break;
    case 'e':
      snprintf(buf, sizeof(buf), fs.alt ? "%#.*e" : "%.*e", prec, av);
      break;
    case 'E':
      snprintf(buf, sizeof(buf), fs.alt ? "%#.*E" : "%.*E", prec, av);
      break;
    case 'g':
      snprintf(buf, sizeof(buf), fs.alt ? "%#.*g" : "%.*g", prec > 0 ? prec : 1, av);
      break;
    case 'G':
      snprintf(buf, sizeof(buf), fs.alt ? "%#.*G" : "%.*G", prec > 0 ? prec : 1, av);
      break;
    case '%':
      snprintf(buf, sizeof(buf), "%.*f%%", prec, av * 100.0);
      break;
    default:
      if (fs.precision >= 0) {
        snprintf(buf, sizeof(buf), "%.*g", prec > 0 ? prec : 1, av);
      } else {
        char *s = _CG_str_from_float(av);
        snprintf(buf, sizeof(buf), "%s", s);
      }
  }
  char core[560];
  snprintf(core, sizeof(core), "%s%s", sign_str, buf);
  char *grouped = fs.group ? _CG_group_digits(core, fs.group) : strdup(core);
  int width = fs.width > 0 ? fs.width : 0;
  char align = fs.align ? fs.align : '>';
  char *result = _CG_pad_align(grouped, width, align, fs.fill, (int)strlen(sign_str));
  free(grouped);
  return result;
}

inline char *_CG_format_int_spec(int64 val, const char *spec_str) {
  _CG_FormatSpec fs;
  _CG_parse_format_spec(spec_str, &fs);
  if (fs.type == 'f' || fs.type == 'F' || fs.type == 'e' || fs.type == 'E' || fs.type == 'g' ||
      fs.type == 'G' || fs.type == '%')
    return _CG_format_float_spec((double)val, spec_str);
  uint64 av = (uint64)(val < 0 ? -val : val);
  const char *sign_str = val < 0 ? "-" : (fs.sign == '+' ? "+" : (fs.sign == ' ' ? " " : ""));
  char buf[80];
  switch (fs.type) {
    case 'x':
      snprintf(buf, sizeof(buf), fs.alt ? "0x%llx" : "%llx", (unsigned long long)av);
      break;
    case 'X':
      snprintf(buf, sizeof(buf), fs.alt ? "0X%llX" : "%llX", (unsigned long long)av);
      break;
    case 'o':
      snprintf(buf, sizeof(buf), fs.alt ? "0o%llo" : "%llo", (unsigned long long)av);
      break;
    case 'b': {
      char tmp[80];
      int ti = 0;
      uint64 uv = av;
      if (uv == 0) tmp[ti++] = '0';
      while (uv) {
        tmp[ti++] = (char)('0' + (uv & 1));
        uv >>= 1;
      }
      int oi = 0;
      if (fs.alt) {
        buf[oi++] = '0';
        buf[oi++] = 'b';
      }
      for (int i = ti - 1; i >= 0; i--) buf[oi++] = tmp[i];
      buf[oi] = 0;
      break;
    }
    case 'c':
      buf[0] = (char)val;
      buf[1] = 0;
      break;
    default:
      snprintf(buf, sizeof(buf), "%llu", (unsigned long long)av);
  }
  char core[160];
  snprintf(core, sizeof(core), "%s%s", sign_str, buf);
  char *grouped = fs.group ? _CG_group_digits(core, fs.group) : strdup(core);
  int width = fs.width > 0 ? fs.width : 0;
  char align = fs.align ? fs.align : '>';
  char *result = _CG_pad_align(grouped, width, align, fs.fill, (int)strlen(sign_str));
  free(grouped);
  return result;
}

inline char *_CG_format_str_spec(const char *val, const char *spec_str) {
  _CG_FormatSpec fs;
  _CG_parse_format_spec(spec_str, &fs);
  const char *core = val;
  char *trunc = 0;
  if (fs.precision >= 0 && (int)strlen(val) > fs.precision) {
    trunc = (char *)malloc((size_t)fs.precision + 1);
    memcpy(trunc, val, (size_t)fs.precision);
    trunc[fs.precision] = 0;
    core = trunc;
  }
  char align = fs.align ? fs.align : '<';
  int width = fs.width > 0 ? fs.width : 0;
  char *result = _CG_pad_align(core, width, align, fs.fill, 0);
  if (trunc) free(trunc);
  return result;
}

inline char *_CG_string_mult(char *str, int64 n) {
  size_t l = _CG_string_len(str);
  char *ret = _CG_string_alloc(l * n);
  for (int64 i = 0; i < n; i++) memcpy(ret + l * i, str, l);
  return ret;
}

inline void *_CG_prim_primitive_clone(void *p, size_t s) {
  void *x = GC_MALLOC(s);
  memcpy(x, p, s);
  return x;
}

// Issue 026 fix: clone with destination-driven allocation.
// When pyc's per-CreationSet struct synthesis produces a
// different (typically smaller) layout for the prototype
// vs the actual instance — happens when the class has >1
// self-typed field, where the proto's CS never receives
// field writes — the destination size must drive the
// GC_MALLOC so subsequent __init__ writes have room.  The
// source's data is copied within min(src, dst) bytes;
// fields not present in the source remain GC-zeroed and
// will be written by __init__.
inline void *_CG_prim_primitive_clone_dst(void *p, size_t dst_sz,
                                                 size_t src_sz) {
  void *x = GC_MALLOC(dst_sz);
  size_t n = src_sz < dst_sz ? src_sz : dst_sz;
  if (p && n) memcpy(x, p, n);
  return x;
}

inline void *_CG_prim_primitive_clone_vector(void *p, size_t s, size_t v) {
  void *x = GC_MALLOC(s + v);
  memcpy(x, p, s);
  memset(((char *)x) + s, 0, v);
  return x;
}

inline char *_CG_strcat(const char *a, const char *b) {
  size_t la = _CG_string_len(a), lb = _CG_string_len(b);
  char *x = _CG_string_alloc(la + lb);
  memcpy(x, a, la);
  memcpy(x + la, b, lb);
  return x;
}

inline char *_CG_char_from_string(void *s, int i) {
  char *x = _CG_string_alloc(1);
  x[0] = ((char *)s)[i];
  return x;
}

#ifdef __cplusplus
static inline char *_CG_prim_primitive_to_string(double d) {
  char s[100], *p = s;
  snprintf(s, 100, "%.17g", d);
  while (*p)
    if (*p < '0' || *p > '9')
      break;
    else
      p++;
  if (!*p) {
    *p++ = '.';
    *p++ = '0';
  } else
    while (*p) p++;
  char *r = _CG_string_alloc(p - s);
  memcpy(r, s, p - s);
  return r;
}

static inline char *_CG_prim_primitive_to_string(int32 i) {
  char s[100];
  snprintf(s, 100, "%d", i);
  return _CG_String(s);
}

static inline char *_CG_prim_primitive_to_string(int64 i) {
  char s[100];
  snprintf(s, 100, "%lld", i);
  return _CG_String(s);
}

static inline int _CG_float_printf(double d, bool ln) {
  char *s = _CG_prim_primitive_to_string(d);
  fputs(s, stdout);
  if (ln) fputs("\n", stdout);
  return 0;
}
#endif  /* __cplusplus */

/*
  Lists and Tuples

  Tuples are stored as a pointer directly to the structure
  containing the tuple elements.

  Lists are stored as _CG_list_struct and a _CG_list
  is a pointer to &((_CG_list_struct*)list)->x[0]

  This makes immutable/constant Lists and Tuples compatible
  and puts the elements of such lists in the same cache line as
  the list information.
*/

typedef struct _CG_list_struct {
  uint32 total_len;
  uint32 len;
  void *ptr;
  char data[4];  // preallocated space
} _CG_list_struct;

#define SIZEOF_LIST_HEADER (sizeof(void *) + 8)

#define _CG_TUPLE_TO_LIST_FUN(_s, _n)                                             \
  static inline _CG_list _CG_to_list(_CG_ps##_s p) {                              \
    _CG_list x = _CG_ptr_to_list(MALLOC(SIZEOF_LIST_HEADER + sizeof(_CG_s##_s))); \
    _CG_list_len(x) = _n;                                                         \
    _CG_list_total_len(0, x) = _n;                                                \
    _CG_list_ptr(x) = _CG_list_data(x);                                           \
    memcpy(x, p, sizeof(_CG_s##_s));                                              \
    return x;                                                                     \
  }

#define _CG_list_to_struct(_l) ((_CG_list_struct *)(((char *)(_l)) - SIZEOF_LIST_HEADER))
#define _CG_list_len(_l) (_CG_list_to_struct(_l)->len)
#define _CG_list_total_len(_c, _l) (_CG_list_to_struct(_l)->total_len)
#define _CG_list_ptr(_l) (_CG_list_to_struct(_l)->ptr)
#define _CG_list_data(_l) (&_CG_list_to_struct(_l)->data[0])
#define _CG_prim_len(_c, _l) ((_l) ? _CG_list_len(_l) : 0)
#define _CG_ptr_to_list(_l) ((_CG_list)(((char *)(_l)) + SIZEOF_LIST_HEADER))
static inline _CG_list _CG_to_list(_CG_list l) { return l; }


static inline _CG_list _CG_list_add_internal(_CG_list l1, _CG_list l2, uint32 size1, uint32 size2) {
  uint32 s1 = _CG_prim_len(0, l1), s2 = _CG_prim_len(0, l2);
  uint32 size = size1 ? size1 : size2;
  _CG_list x = (_CG_list)MALLOC(size * (s1 + s2));
  if (s1) memcpy(x, _CG_list_ptr(l1), s1 * size);
  if (s2) memcpy(((char *)x) + s1 * size, _CG_list_ptr(l2), s2 * size);
  _CG_list_len(l1) = s1 + s2;
  _CG_list_total_len(0, l1) = s1 + s2;
  _CG_list_ptr(l1) = x;
  return l1;
}

static inline _CG_list _CG_list_resize_internal(_CG_list l1, uint32 size1, uint32 new_len) {
  uint32 s1 = _CG_prim_len(0, l1);
  _CG_list x = new_len ? (_CG_list)MALLOC(size1 * new_len) : 0;
  uint32 y = s1 < new_len ? s1 : new_len;
  if (y) memcpy(x, _CG_list_ptr(l1), s1 * size1);
  if (new_len > s1) memset(((char *)x) + s1 * size1, 0, (new_len - s1) * size1);
  _CG_list_len(l1) = new_len;
  _CG_list_total_len(0, l1) = new_len;
  _CG_list_ptr(l1) = x;
  return l1;
}

static inline _CG_list _CG_list_mult_internal(_CG_list l1, uint32 l, uint32 size) {
  if (!l) return 0;
  uint32 s1 = _CG_prim_len(0, l1);
  _CG_list x = _CG_ptr_to_list((_CG_list)MALLOC(size * s1 * l + SIZEOF_LIST_HEADER));
  _CG_list_len(x) = s1 * l;
  _CG_list_total_len(0, x) = s1 * l;
  _CG_list_ptr(x) = x;
  for (int i = 0; i < l; i++) memcpy(((char *)x) + (i * size * s1), _CG_list_ptr(l1), s1 * size);
  return x;
}

static inline _CG_list _CG_list_getslice_internal(_CG_list v, uint32 size, int32 l, int32 h, int32 s) {
  uint32 len = _CG_prim_len(0, v);
  if (l > len) l = len;
  if (l < 0) {
    l = len + l;
    if (l < 0) l = 0;
  }
  if (h > len) h = len;
  if (h < 0) {
    h = len + h;
    if (h < 0) h = 0;
  }
  if (l > h) h = l;
  int n = h - l;
  n = n / s;
  _CG_list x = _CG_ptr_to_list((_CG_list)MALLOC(size * n + SIZEOF_LIST_HEADER));
  _CG_list_len(x) = n;
  _CG_list_total_len(0, x) = n;
  _CG_list_ptr(x) = x;
  if (n) {
    if (s == 1)
      memcpy(x, ((char *)_CG_list_ptr(v)) + l * size, n * size);
    else
      for (int i = 0; i < n; i++) memcpy(((char *)x) + i * size, ((char *)_CG_list_ptr(v)) + (l + i) * size, size);
  }
  return x;
}

static inline _CG_list _CG_list_setslice_internal(_CG_list l1, uint32 size, int32 l, int32 h, _CG_list l2) {
  uint32 len1 = _CG_prim_len(0, l1), len2 = _CG_prim_len(0, l2);
  if (l > len1) l = len1;
  if (l < 0) {
    l = len1 + l;
    if (l < 0) l = 0;
  }
  if (h > len1) h = len1;
  if (h < 0) {
    h = len1 + h;
    if (h < 0) h = 0;
  }
  if (l > h) h = l;
  int s = h - l;         // size to delete
  s = len1 - s;          // size to save
  int new_s = s + len2;  // new size
  _CG_list p1 = _CG_list_ptr(l1);
  _CG_list x = (_CG_list)MALLOC(size * new_s);
  _CG_list_len(l1) = new_s;
  _CG_list_total_len(0, l1) = new_s;
  _CG_list_ptr(l1) = x;
  char *p = (char *)x;
  if (l) {
    memcpy(p, ((char *)p1), l * size);
    p += l * size;
  }
  if (len2) {
    memcpy(p, _CG_list_ptr(l2), len2 * size);
    p += len2 * size;
  }
  int sh = len1 - h;
  if (sh) {
    memcpy(p, ((char *)p1) + h * size, sh * size);
    p += sh * size;
  }
  return l1;
}

inline void *_CG_prim_tuple_list_internal(uint s, uint n) {
  _CG_list x = _CG_ptr_to_list(GC_MALLOC(s * n + SIZEOF_LIST_HEADER));
  _CG_list_len(x) = n;
  _CG_list_total_len(0, x) = n;
  _CG_list_ptr(x) = x;
  return x;
}

inline void _CG_write(const void *s) { if (s) _CG_Syscall_Write(1, s, _CG_string_len(s)); }
inline void _CG_writeln(void) { _CG_Syscall_Write(1, "\n", 1); }

#define _CG_prim_tuple_list(_c, _n) (_c)(_CG_prim_tuple_list_internal(sizeof(*((_c)0)), _n))
#define _CG_prim_list(_e, _n) _CG_prim_tuple_list_internal(sizeof(_e), _n)
#define _CG_prim_tuple(_c, _n) (_c) GC_MALLOC(sizeof(*((_c)0)))
#define _CG_list_add(_l1, _l2, _s1, _s2) (_CG_list_add_internal(_CG_to_list(_l1), _CG_to_list(_l2), _s1, _s2))
#define _CG_list_resize(_l1, _s1, _new_len) (_CG_list_resize_internal(_CG_to_list(_l1), _s1, _new_len))
#define _CG_list_mult(_l1, _l, _s) (_CG_list_mult_internal(_CG_to_list(_l1), _l, _s))
#define _CG_list_getslice(_l, _s, _lower, _upper, _step) \
  (_CG_list_getslice_internal(_CG_to_list(_l), _s, _lower, _upper, _step))
#define _CG_list_setslice(_l1, _s, _lower, _upper, _l2) \
  (_CG_list_setslice_internal(_l1, _s, _lower, _upper, _CG_to_list(_l2)))
#define _CG_prim_coerce(_t, _v) ((_t)_v)
#define _CG_prim_closure(_c) (_c) GC_MALLOC(sizeof(*((_c)0)))
#define _CG_prim_vector(_c, _n) (void *)GC_MALLOC(sizeof(_c *) * _n)
#define _CG_prim_new(_c) (_c) GC_MALLOC(sizeof(*((_c)0)))
#define _CG_prim_clone(_c) _CG_prim_primitive_clone(_c, sizeof(*(_c)))
// Issue 026: dst-sized clone macro — _dt is the destination
// type, _src is the source pointer.  See
// _CG_prim_primitive_clone_dst above for rationale.
#define _CG_prim_clone_dst(_dt, _src) \
  _CG_prim_primitive_clone_dst((_src), sizeof(*((_dt)0)), sizeof(*(_src)))
#define _CG_prim_copy_dst(_dt, _src) \
  _CG_prim_primitive_clone_dst((_src), sizeof(*((_dt)0)), sizeof(*(_src)))
#define _CG_prim_clone_vector(_c, _v) _CG_prim_primitive_clone_vector(_c, sizeof(*(_c)), _v)
#define _CG_prim_reply(_s, _c, _r) return _r
#define _CG_prim_primitive(_p, _x) printf("%d\n", (unsigned int)(uintptr_t)_x);
#define _CG_prim_add(_a, _op, _b) ((_a) + (_b))
#define _CG_prim_subtract(_a, _op, _b) ((_a) - (_b))
#define _CG_prim_rsh(_a, _op, _b) ((_a) >> (_b))
#define _CG_prim_lsh(_a, _op, _b) ((_a) << (_b))
#define _CG_prim_mult(_a, _op, _b) ((_a) * (_b))
#define _CG_prim_mod(_a, _op, _b) ((_a) % (_b))
#define _CG_prim_pow(_a, _op, _b) (pow((_a), (_b)))
#define _CG_prim_div(_a, _op, _b) ((_a) / (_b))
#define _CG_prim_and(_a, _op, _b) ((_a) & (_b))
#define _CG_prim_xor(_a, _op, _b) ((_a) ^ (_b))
#define _CG_prim_or(_a, _op, _b) ((_a) | (_b))
#define _CG_prim_lor(_a, _op, _b) ((_a) || (_b))
#define _CG_prim_land(_a, _op, _b) ((_a) && (_b))
#define _CG_prim_lnot(_op, _a) (!(_a))
#define _CG_prim_less(_a, _op, _b) ((_a) < (_b))
#define _CG_prim_lessorequal(_a, _op, _b) ((_a) <= (_b))
#define _CG_prim_greater(_a, _op, _b) ((_a) > (_b))
#define _CG_prim_greaterorequal(_a, _op, _b) ((_a) >= (_b))
#define _CG_prim_equal(_a, _op, _b) ((_a) == (_b))
#define _CG_prim_notequal(_a, _op, _b) ((_a) != (_b))
inline _CG_bool _CG_str_eq(const char *a, const char *b) { return (_CG_bool)(strcmp(a, b) == 0); }
inline _CG_bool _CG_str_ne(const char *a, const char *b) { return (_CG_bool)(strcmp(a, b) != 0); }
inline _CG_bool _CG_str_lt(const char *a, const char *b) { return (_CG_bool)(strcmp(a, b) < 0); }
inline _CG_bool _CG_str_le(const char *a, const char *b) { return (_CG_bool)(strcmp(a, b) <= 0); }
inline _CG_bool _CG_str_gt(const char *a, const char *b) { return (_CG_bool)(strcmp(a, b) > 0); }
inline _CG_bool _CG_str_ge(const char *a, const char *b) { return (_CG_bool)(strcmp(a, b) >= 0); }
#define _CG_prim_paren(_f, _a) ((*(_f))((_f), (_a)))
#define _CG_prim_set(_a, _b) (_a) = (_b)
#define _CG_prim_minus(_op, _a) (-(_a))
#define _CG_prim_not(_op, _a) (~(_a))
#define _CG_prim_strcat(_a, _op, _b) (_CG_strcat(_a, _b))
#define _CG_prim_apply(_a, _b) ((*(_a)->e0)((_a)->e1))
#define _CG_make_apply(_r, _s, _f, _a)    \
  do {                                    \
    _r = (_s)GC_MALLOC(sizeof(*((_s)0))); \
    _r->e0 = _f;                          \
    _r->e1 = _a;                          \
  } while (0)
inline char *_CG_chr(int x) {
  unsigned char *s = (unsigned char *)_CG_string_alloc(1);
  s[0] = (unsigned char)x;
  return (char *)s;
}
inline int _CG_ord(char *x) {
  if (x)
    return *(unsigned char *)x;
  else
    return 0;
}
