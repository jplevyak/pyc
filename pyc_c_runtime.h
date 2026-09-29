#pragma once

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#ifdef __cplusplus
#include <type_traits>
#endif
#include <time.h>

#include "gc.h"

/* PYC_NO_GC: route the GENERATED program's allocations to calloc instead
 * of the collector, so they live in malloc's heap where valgrind can see
 * them exactly. Boehm is deliberately valgrind-hostile -- it scans memory
 * conservatively and reads uninitialised bytes by design -- so a heap bug
 * in emitted code surfaces only as a mysterious crash inside
 * GC_clear_fl_marks / GC_set_fl_marks on a garbage free-list pointer,
 * with no indication of who corrupted it. Under this switch the same
 * out-of-bounds write lands in a malloc block and valgrind names the
 * culprit line.
 *
 * calloc, not malloc: GC_MALLOC returns ZEROED memory and the emitted
 * code relies on that (a fresh object's unwritten fields must read as
 * null/0). Plain malloc would hand back garbage and manufacture failures
 * that have nothing to do with the bug being chased.
 *
 * The collector stays linked and initialised -- pyc_runtime.o and
 * libifa_gc.a use it -- it simply ends up managing almost nothing. This
 * LEAKS by construction; it is a debugging mode, not a runtime option.
 */
#ifdef PYC_NO_GC
#undef GC_MALLOC
#undef GC_MALLOC_ATOMIC
#undef GC_REALLOC
#define GC_MALLOC(sz) calloc(1, (size_t)(sz))
#define GC_MALLOC_ATOMIC(sz) calloc(1, (size_t)(sz))
#define GC_REALLOC(p, sz) realloc((p), (size_t)(sz))
#endif
#include <sys/time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <poll.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <dirent.h>

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
int _CG_net_socket(int family, int type, int proto);
int _CG_net_bind(int fd, const char* host, int port);
int _CG_net_listen(int fd, int backlog);
int _CG_net_accept(int fd);
int _CG_net_close(int fd);
int _CG_net_poll_read(int fd, int timeout_ms);

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

// Generator support (issues/014): a Python generator function is
// compiled as a C++20 coroutine, same family as _CG_Coroutine above,
// but driven synchronously by __next__/__pyc_more__/.send() (pyc's
// existing iterator protocol, see __pyc__/07_file.py's __file_iter__)
// instead of an event loop -- no awaiter chain, no _CG_event_loop_*
// calls. _CG_generator_advance/_CG_generator_send are the only entry
// points: each resumes the coroutine once (unless already done, where
// resuming would be UB) and reports whether a value is now available.
// Callers must always pair one advance()/send() with at most one
// _CG_generator_value() read before the next advance()/send() --
// __pyc_generator__ (09_generator.py) tracks this with its own
// "primed" flag so both the __pyc_more__-then-__next__ alternation
// PY_for_stmt's lowering does, and bare repeated __next__()/.send()
// calls with no __pyc_more__ in between, stay one-for-one.
//
// `sent`: the value delivered to a paused `x = yield foo` expression
// by the *next* resume. A plain advance (bare next()) delivers None
// (nullptr); .send(v) delivers v. yield_value's returned awaiter's
// await_resume() is what `co_yield expr`'s own expression value
// becomes in the coroutine body -- previously this returned void
// (std::suspend_always's await_resume()), which is why `x = yield
// foo` used to be hardcoded to None at the frontend (PY_yield_expr)
// with no way to actually deliver a sent value; the custom
// yield_awaiter below is what makes that real.
struct _CG_Generator {
  struct promise_type {
    void* value = nullptr;
    void* sent = nullptr;
    // issues/014: the generator's `return value` (StopIteration.value
    // in real Python) -- for BOTH a bare/fall-through exit (holds
    // gen_fun_pyda's int64 placeholder) and an explicit `return X`
    // (holds X): both flow a real value through fn->ret at the IF1
    // level already (only the bare case needed FA-type-anchoring
    // help), so return_value(T) below can handle them uniformly. A
    // C++ coroutine promise may define return_void() OR return_value
    // (T), never both -- switching to return_value(T) here (from the
    // old return_void()) is what makes `co_return X;` legal for
    // is_generator functions at all; cg.cc's P_prim_reply handling
    // was updated to always pass a value, matching is_async.
    void* retval = nullptr;

    struct yield_awaiter {
      promise_type* p;
      bool await_ready() const noexcept { return false; }
      void await_suspend(std::coroutine_handle<>) const noexcept {}
      void* await_resume() const noexcept { return p->sent; }
    };

    _CG_Generator get_return_object() {
      return _CG_Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }

    template<typename T>
    yield_awaiter yield_value(T v) { value = (void*)(uintptr_t)v; return yield_awaiter{this}; }
    template<typename T>
    void return_value(T v) { retval = (void*)(uintptr_t)v; }
    void unhandled_exception() {}
  };

  std::coroutine_handle<promise_type> handle;
};

// int64 in/out at this boundary, matching every other opaque-handle
// runtime helper (_CG_fopen/_CG_fclose et al. smuggle a FILE* through
// int64 the same way) -- __pyc_c_call__ sites deal in plain int64,
// never a raw pointer or C++ type. `int64`'s typedef comes later in
// this header (outside the __cplusplus coroutine block above), so
// `long long` is used directly here rather than reordering the file.
inline bool _CG_generator_advance(long long raw_handle) {
  auto h = std::coroutine_handle<_CG_Generator::promise_type>::from_address((void*)(intptr_t)raw_handle);
  if (h.done()) return false;
  h.promise().sent = nullptr;
  h.resume();
  return !h.done();
}

// .send(v): like _CG_generator_advance, but delivers `v` as the
// value of the paused `yield` expression being resumed into, instead
// of None. Calling .send() on a not-yet-started generator (no paused
// yield to deliver into yet) is the same UB-if-done guard as advance
// -- real Python raises a TypeError for that specific case, which
// pyc has no exception model to express (issue 011); left unchecked,
// matching every other iterator's past-exhaustion behavior here.
inline bool _CG_generator_send(long long raw_handle, long long value) {
  auto h = std::coroutine_handle<_CG_Generator::promise_type>::from_address((void*)(intptr_t)raw_handle);
  if (h.done()) return false;
  h.promise().sent = (void*)(uintptr_t)value;
  h.resume();
  return !h.done();
}

// issues/114: an identity on the coroutine handle whose only job is to
// be OPAQUE to flow analysis.
//
// A generator function's IF1 return value and its C return value are
// two different things: FA sees whatever the body's yields/returns put
// in fn->ret, while cg.cc overrides the emitted `return` to hand back
// the coroutine handle. That divergence is normally harmless, but a
// generator with a single constant yield (`yield 1`, then a raise or a
// fall-through) makes FA's view of the return a SINGLETON CONSTANT --
// and the caller then inlines that literal in place of the call's
// result (ifa/optimize/dead.cc's get_constant), so the wrapper
// constructed __pyc_generator__(1) and every later resume dereferenced
// 1 as a coroutine frame. Routing the handle through this call, whose
// declared FA type is a plain `int` meta type rather than an example
// value, gives the wrapper an abstract non-constant int to carry the
// handle in, leaving the call's own result free to carry the yielded
// TYPE (which is all __pyc_generator__ wants from it).
//
// Same family as issues/022's P_prim_await liveness fix: a coroutine
// handle is not a value FA may reason about by its contents.
inline long long _CG_generator_handle(long long raw_handle) { return raw_handle; }

inline long long _CG_generator_value(long long raw_handle) {
  auto h = std::coroutine_handle<_CG_Generator::promise_type>::from_address((void*)(intptr_t)raw_handle);
  return (long long)(uintptr_t)h.promise().value;
}

// issues/171 #7: there is no _CG_generator_return_value any more. The
// body stores its `return X` in its own __pyc_generator__ object
// (09_generator.py's `retval`), typed per generator; the co_return value
// is no longer read.

// issues/014: a coroutine body with no EXPLICIT `return X` anywhere
// (bare fall-through, or a bare `return`) still needs *some* int64
// value flowing into fn->ret purely so FA infers an int return type
// for the Fun -- codegen (cg.cc, is_generator) now passes whatever's
// there through to promise_type::return_value uniformly (see above),
// so this placeholder's value doubles as that generator's reported
// StopIteration.value == 0 in the no-explicit-return case (real
// Python reports None there instead -- a deliberate v1 compromise,
// same "smuggle through int64" scope as yield/send values).
//
// A literal FA constant here gets constant-folded all the way through
// a caller (the synthesized wrapper, see python_ifa_build_if1.cc's
// PY_funcdef case), collapsing the real, dynamic coroutine handle to
// the same fake value everywhere. Routing the placeholder through a
// genuine opaque C call (built via the same IF1 shape __pyc_c_call__
// produces, see gen_fun_pyda) keeps it int64-typed without FA
// believing it knows the value.
inline long long _CG_generator_placeholder_return() { return 0; }

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

// issues/022 follow-up: mirrors _CG_Await_Net_Read/Write -- registers
// a timed wakeup (_CG_event_loop_sleep, pyc_runtime.c) instead of an
// fd, so `await`ing this actually suspends the coroutine and lets the
// event loop's poll() timeout carry it back at (or after) the
// requested time, rather than resuming immediately like a bare
// value-returning stub would.
struct _CG_Await_Sleep {
  double seconds;
  bool await_ready() const noexcept { return false; }
  void await_suspend(std::coroutine_handle<> h) noexcept {
    _CG_event_loop_sleep(h.address(), seconds);
  }
  void await_resume() const noexcept {}
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
// bytes shares str's exact length-prefixed char* buffer layout (see
// sym_bytes registration, ifa/if1/ast.cc) -- same representation, distinct
// C type name so the two stay type-checked separately by the C compiler.
typedef char *_CG_bytes;
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

// issue 011: the None-check route (isinstance(x, NoneType), what
// `x is None` lowers to) stays a simple null test. A real-class
// check against a record's classtag is NOT expressible as a single
// comparison here (a class can have subclasses whose classtag
// differs by identity) -- cg.cc/cg_emit_llvm.cc emit a compile-time
// disjunction over the class's implementors (FA's own subclass set,
// same one fa.cc's constant-folding isinstance uses) directly at the
// call site instead of calling through this macro for that case.
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

// Length-aware sibling of _CG_String (ifa/issues/070): `x` is a C string
// literal escaped from a str/bytes constant that may contain an embedded
// NUL byte (from a \x00/\0 escape) -- strlen() on it would stop at that
// NUL regardless of what follows in the source text, since a C string
// literal is fundamentally NUL-terminated at the language level no
// matter how faithfully the compiler escaped it. `len` (an integer
// literal codegen emits alongside `x`) carries the true extent through
// this final materialization step.
inline char *_CG_String_n(const void *x, size_t len) {
  char *str = _CG_string_alloc(len);
  memcpy(str, x, len);
  return str;
}

// Real process argv (ifa/issues -- pyc_lib/sys.py used to hardcode
// `argv = ["pyc"]`, a compile-time Python list literal FA constant-
// folds through: any `if len(sys.argv) > 1: ...` collapsed to its
// `else` branch at compile time, regardless of what's actually passed
// on the real command line, silently dead-coding the other branch.
// _CG_set_argv is called once from generated main() before anything
// else runs; _CG_argc/_CG_argv_at expose it to pyc_lib/sys.py's
// `_get_argv()` as ordinary opaque C calls (see __pyc_c_call__), so
// FA sees "some int" / "some str" rather than a known constant and
// can't fold the length away. Storage is `extern`, defined once in
// pyc_runtime.c -- same pattern as _CG_ready_queue_head above -- so
// both backends share one copy (the C backend links pyc_runtime.o
// too, per Makefile.cg).
extern int64 _cg_argc;
extern char **_cg_argv;

inline void _CG_set_argv(int64 argc, char **argv) {
  _cg_argc = argc;
  _cg_argv = argv;
}

inline int64 _CG_argc(void) { return _cg_argc; }

inline char *_CG_argv_at(int64 i) {
  if (i < 0 || i >= _cg_argc || !_cg_argv || !_cg_argv[i]) return _CG_String("");
  return _CG_String(_cg_argv[i]);
}

// os module helpers (ifa/issues/041): pyc_lib/os.py's filesystem
// functions used to be no-op stubs (listdir/walk always [], stat
// always all-zero, isdir/exists hardcoded true). Most of libc's
// filesystem calls (chdir, rename, unlink, mkdir, system, access) are
// directly callable via __pyc_c_call__ as-is -- a pyc `_CG_string` is
// already a valid NUL-terminated `const char*` (see the string-layout
// note above), so no wrapper is needed for those. These few need a
// thin C helper because they either return a struct (`stat`), a
// pointer that must be smuggled through int64 (`opendir`/`readdir`,
// same pattern as `_CG_fopen`'s `FILE*` above), or a fixed-size buffer
// (`getcwd`).

inline char *_CG_getcwd(void) {
  char buf[4096];
  if (getcwd(buf, sizeof(buf))) return _CG_String(buf);
  return _CG_String("");
}

inline _CG_bool _CG_is_dir(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

inline _CG_bool _CG_is_symlink(const char *path) {
  struct stat st;
  return lstat(path, &st) == 0 && S_ISLNK(st.st_mode);
}

// Tuple-view field order matches CPython's own documented
// backward-compat 10-tuple for os.stat_result: (mode, ino, dev,
// nlink, uid, gid, size, atime, mtime, ctime). CPython's tuple view
// truncates the times to whole-second integers (the float precision
// only exists via the named st_atime/etc. attributes) -- matching
// that, rather than returning float, keeps this a uniform all-int64
// tuple on the pyc side: a mixed int/str-shaped (or here int/float)
// tuple has no working generic __str__ in pyc (ifa/issues/018).
inline int64 _CG_stat_int_field(const char *path, int64 field) {
  struct stat st;
  if (stat(path, &st) != 0) return 0;
  switch (field) {
    case 0: return (int64)st.st_mode;
    case 1: return (int64)st.st_ino;
    case 2: return (int64)st.st_dev;
    case 3: return (int64)st.st_nlink;
    case 4: return (int64)st.st_uid;
    case 5: return (int64)st.st_gid;
    case 6: return (int64)st.st_size;
    case 7: return (int64)st.st_atime;
    case 8: return (int64)st.st_mtime;
    case 9: return (int64)st.st_ctime;
    default: return 0;
  }
}

inline int64 _CG_opendir(const char *path) { return (int64)(intptr_t)opendir(path); }
inline int64 _CG_closedir(int64 h) { return h ? (int64)closedir((DIR *)(intptr_t)h) : 0; }

// Returns "" both on a real end-of-directory (readdir returns NULL)
// and on a genuine empty-name entry, which never happens in practice
// (every real dirent has a non-empty d_name) -- so "" is an
// unambiguous-in-practice sentinel for pyc_lib/os.py's listdir loop,
// matching the same convention _CG_argv_at-style helpers use
// elsewhere in this header.
inline char *_CG_readdir_name(int64 h) {
  if (!h) return _CG_String("");
  struct dirent *e = readdir((DIR *)(intptr_t)h);
  if (!e) return _CG_String("");
  return _CG_String(e->d_name);
}

// issues/165: Python's `%d` is C's `int` conversion -- 32 bits -- but
// every integer pyc pushes into this vararg list is widened to int64 (see
// format_string_codegen in python_ifa_main.cc and the matching emitter in
// cg_emit_llvm.cc). Handing the Python format string to vsnprintf
// unchanged therefore read 32 bits of a 64-bit argument:
//
//   "%d" % 199999990000000   printed  542894464
//
// which is exactly 199999990000000 mod 2**32, silently, with `print(n)`
// and `"%s" % n` both correct. Insert the `ll` length modifier into every
// integer conversion so the conversion matches the argument it reads.
//
// Done here rather than in the two backends' emitters because this is the
// one place both of them meet, and because it also covers a format string
// that is NOT a compile-time constant, which neither emitter can parse.
// `%c` is deliberately left alone: C's `%c` takes an `int`, and the
// emitters cast that argument to `(int)` for exactly this reason.
//
// Returns `fmt` itself when there is nothing to rewrite, so the common
// case allocates nothing.
inline const char *_CG_widen_int_convs(const char *fmt) {
  int n = 0;
  for (const char *p = fmt; *p; p++)
    if (p[0] == '%') {
      if (p[1] == '%') { p++; continue; }
      n++;
    }
  if (!n) return fmt;
  /* worst case: every '%' introduces an integer conversion (+2 chars) */
  char *out = (char *)GC_MALLOC_ATOMIC(strlen(fmt) + 2 * (size_t)n + 1);
  char *o = out;
  for (const char *p = fmt; *p;) {
    if (*p != '%') { *o++ = *p++; continue; }
    *o++ = *p++;            /* the '%' */
    if (*p == '%') { *o++ = *p++; continue; }
    /* flags, width, precision -- copied through unchanged */
    while (*p && (strchr("-+ #0", *p) || (*p >= '0' && *p <= '9') || *p == '.')) *o++ = *p++;
    /* drop any length modifier the format already carries, so a
       hand-written "%ld" does not become "%lldd" */
    while (*p && strchr("hlLqjzt", *p)) p++;
    if (*p && strchr("diouxX", *p)) { *o++ = 'l'; *o++ = 'l'; }
    if (*p) *o++ = *p++;
  }
  *o = 0;
  return out;
}

// issues/165 (the half left open): a format string that is NOT a
// compile-time constant.
//
// The emitters cast each argument to what its conversion needs -- but only
// when they can PARSE the format to know which conversion that is. With a
// non-constant format they cannot, so `conv` is 0 for every argument and
// `format_string_emit_cast` widens an integer to int64 and leaves a float
// alone. A double then meets `%d`, which on x86-64 SysV reads an integer
// register while the value sits in an xmm one: `"%d" % 3.7` through a
// computed format printed `25637`, and a different number next run.
//
// The emitter cannot know the conversion, and the runtime cannot know the
// argument's type -- so the emitter passes the TYPES and the runtime,
// which already walks the format for _CG_widen_int_convs, pairs them up.
// `tags` has one character per argument: 'i' integer (passed as int64),
// 'f' float (passed as double), 's' anything else (passed as a pointer).
//
// This formats one conversion at a time instead of handing the whole
// format to vsnprintf, because the mismatch cannot be repaired by
// rewriting the format alone: CPython's `"%d" % 3.7` is `3`, a TRUNCATION,
// and no printf conversion truncates a double. The value has to be read as
// a double and converted, which is what the constant path's `(int64)` cast
// does and what this does per argument.
//
// The CONSTANT path is untouched and still goes through _CG_format_string.
inline char *_CG_fmt_grow(char *out, size_t *cap, size_t len, size_t need) {
  if (len + need + 1 <= *cap) return out;
  while (len + need + 1 > *cap) *cap *= 2;
  {
    char *nb = (char *)GC_MALLOC_ATOMIC(*cap);
    memcpy(nb, out, len);
    return nb;
  }
}

inline char *_CG_str_from_float(double d);  /* defined below; used by the %s path */

inline char *_CG_format_string_tagged(char *str, const char *tags, ...) {
  size_t cap = _CG_string_len(str) + 64, len = 0;
  char *out = (char *)GC_MALLOC_ATOMIC(cap);
  const char *p = str;
  int ti = 0;
  va_list ap;
  va_start(ap, tags);
  while (*p) {
    if (*p != '%') {
      out = _CG_fmt_grow(out, &cap, len, 1);
      out[len++] = *p++;
      continue;
    }
    {
      char spec[64];
      int si = 0;
      char conv, tag;
      char sbuf[512];
      char *big = 0;
      int m;
      spec[si++] = *p++;                       /* the '%' */
      if (*p == '%') {
        out = _CG_fmt_grow(out, &cap, len, 1);
        out[len++] = '%';
        p++;
        continue;
      }
      /* flags, width, precision -- copied through unchanged */
      while (*p && si < 56 && (strchr("-+ #0", *p) || (*p >= '0' && *p <= '9') || *p == '.')) spec[si++] = *p++;
      /* a length modifier the source wrote is dropped; we supply our own */
      while (*p && strchr("hlLqjzt", *p)) p++;
      if (!*p) break;
      conv = *p++;
      tag = tags && tags[ti] ? tags[ti] : 's';
      ti++;
      if (strchr("diouxX", conv)) {
        long long v = (tag == 'f') ? (long long)va_arg(ap, double) : (long long)va_arg(ap, int64);
        spec[si++] = 'l'; spec[si++] = 'l'; spec[si++] = conv; spec[si] = 0;
        m = snprintf(sbuf, sizeof(sbuf), spec, v);
        if (m >= (int)sizeof(sbuf)) { big = (char *)GC_MALLOC_ATOMIC((size_t)m + 1); snprintf(big, (size_t)m + 1, spec, v); }
      } else if (strchr("feEgGFaA", conv)) {
        double v = (tag == 'f') ? va_arg(ap, double) : (double)va_arg(ap, int64);
        spec[si++] = conv; spec[si] = 0;
        m = snprintf(sbuf, sizeof(sbuf), spec, v);
        if (m >= (int)sizeof(sbuf)) { big = (char *)GC_MALLOC_ATOMIC((size_t)m + 1); snprintf(big, (size_t)m + 1, spec, v); }
      } else if (conv == 'c') {
        /* C's %c consumes an int, so this one NARROWS where the others widen */
        int v = (tag == 'f') ? (int)va_arg(ap, double) : (int)va_arg(ap, int64);
        spec[si++] = conv; spec[si] = 0;
        m = snprintf(sbuf, sizeof(sbuf), spec, v);
      } else {
        /* A NUMBER meeting `%s`. CPython prints `str(x)` -- `"%s" % 42` is
           "42" -- and with a constant format the frontend pre-converts the
           argument through __str__ before it ever gets here. It cannot do
           that without the format, so the number arrived raw and `%s` read
           it AS A POINTER: `"%s" % 42` through a computed format
           SEGFAULTED. The tag says what it really is, so render it here and
           let the %s spec's width/flags apply to the rendered text, which
           is what CPython does too.
           An OBJECT at `%s` is still wrong -- see issues/168. Its tag is
           's' like a string's, and telling them apart would not help: a
           runtime helper cannot call back into __str__. */
        const char *v;
        char numbuf[64];
        if (tag == 'b') {
          v = va_arg(ap, int64) ? "True" : "False";
        } else if (tag == 'i') {
          snprintf(numbuf, sizeof(numbuf), "%lld", (long long)va_arg(ap, int64));
          v = numbuf;
        } else if (tag == 'f') {
          v = _CG_str_from_float(va_arg(ap, double));
        } else {
          v = (const char *)va_arg(ap, void *);
        }
        if (!v) v = "";
        spec[si++] = (conv == 's' || conv == 'p') ? conv : 's';
        spec[si] = 0;
        m = snprintf(sbuf, sizeof(sbuf), spec, v);
        if (m >= (int)sizeof(sbuf)) { big = (char *)GC_MALLOC_ATOMIC((size_t)m + 1); snprintf(big, (size_t)m + 1, spec, v); }
      }
      if (m < 0) m = 0;
      out = _CG_fmt_grow(out, &cap, len, (size_t)m);
      memcpy(out + len, big ? big : sbuf, (size_t)m);
      len += (size_t)m;
    }
  }
  va_end(ap);
  {
    char *s = _CG_string_alloc(len);
    memcpy(s, out, len);
    return s;
  }
}

inline char *_CG_format_string(char *str, ...) {
  const char *fmt = _CG_widen_int_convs(str);
  int l = _CG_string_len(str) + 24;
  char *s = 0;
  va_list ap;
  while (1) {
    va_start(ap, str);
    s = _CG_string_alloc(l);
    int ll = vsnprintf(s, l, fmt, ap);
    va_end(ap);
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

// CPython's float repr (and str, which is the same since 3.2): the
// SHORTEST decimal string that round-trips to the same double, written
// fixed-point when the decimal exponent is in [-4, 16) and scientific
// otherwise, with ".0" on a whole number and at least two exponent
// digits. `%.17g` -- what this used to be -- round-trips too but is not
// shortest: print(0.1) gave 0.10000000000000001. Writes into `out`
// (>= 40 bytes) and returns the length.
static inline int _CG_float_repr_buf(double d, char *out) {
  if (isnan(d)) { strcpy(out, "nan"); return 3; }
  if (isinf(d)) { strcpy(out, d < 0 ? "-inf" : "inf"); return d < 0 ? 4 : 3; }
  if (d == 0) { strcpy(out, signbit(d) ? "-0.0" : "0.0"); return signbit(d) ? 4 : 3; }
  char e[48];
  for (int p = 1; p <= 17; p++) {
    snprintf(e, sizeof(e), "%.*e", p - 1, d);
    if (strtod(e, 0) == d) break;
  }
  // e is [-]D[.DDD]e[+-]XX
  const char *q = e;
  int n = 0;
  if (*q == '-') { out[n++] = '-'; q++; }
  char dig[24];
  int nd = 0;
  for (; *q && *q != 'e'; q++)
    if (*q != '.') dig[nd++] = *q;
  int x = atoi(q + 1);
  while (nd > 1 && dig[nd - 1] == '0') nd--;
  if (x >= -4 && x < 16) {
    if (x >= 0) {
      for (int i = 0; i <= x; i++) out[n++] = i < nd ? dig[i] : '0';
      out[n++] = '.';
      if (nd > x + 1)
        for (int i = x + 1; i < nd; i++) out[n++] = dig[i];
      else
        out[n++] = '0';
    } else {
      out[n++] = '0';
      out[n++] = '.';
      for (int i = 0; i < -x - 1; i++) out[n++] = '0';
      for (int i = 0; i < nd; i++) out[n++] = dig[i];
    }
  } else {
    out[n++] = dig[0];
    if (nd > 1) {
      out[n++] = '.';
      for (int i = 1; i < nd; i++) out[n++] = dig[i];
    }
    n += snprintf(out + n, 8, "e%c%02d", x < 0 ? '-' : '+', x < 0 ? -x : x);
  }
  out[n] = 0;
  return n;
}

// D.5: float -> str. See _CG_float_repr_buf. A unique C-callable name so
// libpyc_runtime.a can export it.
inline char *_CG_str_from_float(double d) {
  char tmp[48];
  int n = _CG_float_repr_buf(d, tmp);
  char *s = _CG_string_alloc(n);
  memcpy(s, tmp, n);
  return s;
}

// str -> float / int parsing for `float("...")` / `int("...")`. A
// _CG_string is a NUL-terminated char* (with side length metadata), so
// strtod/strtoll consume it directly. strtod already accepts CPython's
// "inf"/"-inf"/"nan"/"infinity" spellings and scientific notation.
// Codegen routes here only when a string flows into the coerce
// primitive; a numeric source keeps the plain cast (see
// emit_send_coerce). No exception model yet (issue 011): an unparseable
// string yields 0.0 / 0 rather than raising ValueError.
inline double _CG_str_to_float64(char *s) { return strtod(s, 0); }
inline int64 _CG_str_to_int64(char *s) { return (int64)strtoll(s, 0, 10); }
inline int64 _CG_str_to_int64_base(char *s, int base) { return (int64)strtoll(s, 0, base); }

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

// ifa/issues/165: a `{None, str}` union is a nullable pointer, and the
// runtime's string helpers read NULL as the empty string
// (`_CG_string_len(NULL) == 0`). That turned `None + "y"` into `"y"`, a
// silent wrong answer where CPython raises TypeError. An operation CPython
// rejects on None must say so. pyc has no catchable TypeError from the C
// runtime yet, so this reports it the way an uncaught exception is
// reported (`__pyc_unhandled_exception__`: stdout, exit 1). A `try` around
// it does not catch it -- recorded in ifa/165.
__attribute__((noreturn)) static inline void _CG_none_operand(const char *op, const char *other) {
  fflush(stdout);
  printf("Unhandled exception: unsupported operand type(s) for %s: %s\n", op, other);
  fflush(stdout);
  exit(1);
}

// ifa/issues/165: a method call whose receiver is None where None has no
// such method (codegen emits the check; see nil_receiver_rval). Reports
// CPython's message the way an uncaught exception is reported. `sel` is
// the selector name.
inline void _CG_none_receiver(const char *sel) {
  static const char *const ops[][2] = {
      {"__add__", "+"},   {"__sub__", "-"},   {"__mul__", "*"},      {"__truediv__", "/"},
      {"__floordiv__", "//"}, {"__mod__", "%"}, {"__pow__", "** or pow()"}, {"__lt__", "<"},
      {"__le__", "<="},   {"__gt__", ">"},    {"__ge__", ">="},      {"__and__", "&"},
      {"__or__", "|"},    {"__xor__", "^"},   {"__lshift__", "<<"},  {"__rshift__", ">>"}};
  fflush(stdout);
  if (!sel) sel = "?";
  for (unsigned i = 0; i < sizeof(ops) / sizeof(ops[0]); i++)
    if (!strcmp(sel, ops[i][0])) {
      printf("Unhandled exception: unsupported operand type(s) for %s: 'NoneType'\n", ops[i][1]);
      fflush(stdout);
      exit(1);
    }
  if (!strcmp(sel, "__getitem__") || !strcmp(sel, "__setitem__"))
    printf("Unhandled exception: 'NoneType' object is not subscriptable\n");
  else if (!strcmp(sel, "__len__"))
    printf("Unhandled exception: object of type 'NoneType' has no len()\n");
  else if (!strcmp(sel, "__iter__"))
    printf("Unhandled exception: 'NoneType' object is not iterable\n");
  else
    printf("Unhandled exception: 'NoneType' object has no attribute '%s'\n", sel);
  fflush(stdout);
  exit(1);
}

inline char *_CG_strcat(const char *a, const char *b) {
  if (!a) _CG_none_operand("+", "'NoneType' and 'str'");
  if (!b) _CG_none_operand("+", "'str' and 'NoneType'");
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

// `bytes` indexing counterpart to _CG_char_from_string above: `bytes`
// shares str's exact length-prefixed char* buffer layout (see sym_bytes
// registration, ifa/if1/ast.cc), but CPython's `bytes[i]` yields a plain
// int, not a length-1 bytes object -- so no allocation here, unlike the
// str case (cheaper too).
// A one-byte `bytes` from an int (low 8 bits). struct.pack builds its
// result from these instead of `bytes(list)`, whose conversion's internal
// str lists merged with the int list on the start-merged list contour and
// broke shedskin_examples/sha.
inline char *_CG_byte_from_int(int64 v) {
  char *x = _CG_string_alloc(1);
  x[0] = (char)(v & 255);
  return x;
}

// CPython's str repr: quote with ' unless the text contains ' and no ";
// escape the backslash, \n \r \t and the chosen quote; other control
// bytes (< 0x20, 0x7f) as \xNN. Bytes >= 0x80 are UTF-8 sequences for
// printable non-ASCII characters and pass through. str.__repr__ used to be
// "'" + s + "'" with no escaping at all.
inline char *_CG_str_repr(const char *s) {
  size_t n = _CG_string_len(s);
  int sq = 0, dq = 0;
  for (size_t i = 0; i < n; i++) {
    if (s[i] == '\'') sq = 1;
    else if (s[i] == '"') dq = 1;
  }
  char q = (sq && !dq) ? '"' : '\'';
  size_t m = 2;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == '\\' || c == '\n' || c == '\r' || c == '\t' || c == (unsigned char)q) m += 2;
    else if (c < 0x20 || c == 0x7f) m += 4;
    else m += 1;
  }
  char *x = _CG_string_alloc(m);
  char *o = x;
  static const char hex[] = "0123456789abcdef";
  *o++ = q;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == '\\') { *o++ = '\\'; *o++ = '\\'; }
    else if (c == '\n') { *o++ = '\\'; *o++ = 'n'; }
    else if (c == '\r') { *o++ = '\\'; *o++ = 'r'; }
    else if (c == '\t') { *o++ = '\\'; *o++ = 't'; }
    else if (c == (unsigned char)q) { *o++ = '\\'; *o++ = q; }
    else if (c < 0x20 || c == 0x7f) { *o++ = '\\'; *o++ = 'x'; *o++ = hex[c >> 4]; *o++ = hex[c & 15]; }
    else *o++ = (char)c;
  }
  *o++ = q;
  return x;
}

inline int64 _CG_int_from_string(void *s, int i) { return (int64)(unsigned char)((char *)s)[i]; }

// `str`/`bytes` share one C representation (both are `_CG_string`-shaped
// buffers), so converting between the two Python-level types (str.encode(),
// bytes.decode(), bytes(some_str)) is a pure relabeling -- no bytes moved,
// no allocation. This exists so `__pyc_c_call__` has a named C function to
// call for that relabeling rather than reaching for `_CG_strcat`-style
// helpers that assume two same-typed operands.
inline char *_CG_string_identity(char *s) { return s; }

// issues/025: plain (non-slice) indexing (`a[-1]`, `s[-1]`) had no
// negative-index normalization at all -- str.__getitem__/
// list.__getitem__/list.__setitem__ pass the raw key straight
// through to index_object/set_index_object's C array indexing
// (`array[key]`, `_CG_char_from_string(s, key)`), which is
// undefined behavior for a negative key: it reads/writes memory
// *before* the buffer instead of counting back from the end. `a[-1]`
// on a real list returned a garbage value (whatever happened to sit
// just before the allocation), not the last element -- confirmed
// separately from (and worse than) the slicing bugs above, since
// this is out-of-bounds memory access, not just a wrong-value
// computation. Tried fixing this in the __getitem__/__setitem__
// Python source first (mirroring range.__getitem__'s existing `if
// idx < 0: idx += len(self)`, 05_builtins.py) -- that approach
// broke FA's handling of an empty list literal sharing a program
// with a non-empty one (issues/025/040's "has no type" fragility):
// even a no-op `if key < 0: pass` inside list.__getitem__ was enough
// to trigger it, confirmed by bisecting the Python source down to
// that single comparison. Normalizing here instead -- a plain C
// ternary in the generated code, no FA-visible branch at all --
// sidesteps that fragility entirely.
inline int32 _CG_norm_idx(int32 idx, int32 len) { return idx < 0 ? idx + len : idx; }

// `str` has no __getitem__ that handles a slice key (str.__getitem__
// always calls index_object, a single-char op) -- unlike list/range/
// bytearray, str previously had no __pyc_getslice__ override of its
// own, so `s[i:j]` fell through to __pyc_any_type__'s generic
// self.__getitem__(slice(i,j,s)) fallback, which no __getitem__
// actually handles either (it always ends up passing the slice
// *object* itself where an int index is expected). Mirrors
// _CG_list_getslice_internal's clamp/step logic, but direct on the
// length-prefixed string buffer instead of a list's element array.
//
// An omitted bound (INT_MIN for lower, INT_MAX for upper -- see the
// frontend's int64_constant(INT_MIN)/int64_constant(INT_MAX)
// defaults, python_ifa_build_if1.cc) needs a different real default
// depending on the step's sign: `s[::-1]`'s omitted lower/upper are
// len-1/before-0, not the positive-step 0/len. Mirrors CPython's
// PySlice_GetIndicesEx. A negative step with EXPLICIT bounds also
// clamps differently on overflow (out-of-range lands on len-1/-1,
// not len/0) -- confirmed the previous version's positive-step-only
// clamping made `s[::-1]` compute a negative element count that,
// assigned to the (unsigned) list-length header elsewhere
// (_CG_list_getslice_internal shares this same shape), wrapped to a
// huge value and read/printed as a practically-infinite loop, not
// just wrong output.
inline char *_CG_string_getslice(const char *s, int32 l, int32 h, int32 step) {
  int32 len = (int32)_CG_string_len(s);
  if (!step) step = 1;
  if (l == INT32_MIN) {
    l = step < 0 ? len - 1 : 0;
  } else if (l < 0) {
    l += len;
    if (l < 0) l = step < 0 ? -1 : 0;
  } else if (l >= len) {
    l = step < 0 ? len - 1 : len;
  }
  if (h == INT32_MAX) {
    h = step < 0 ? -1 : len;
  } else if (h < 0) {
    h += len;
    if (h < 0) h = step < 0 ? -1 : 0;
  } else if (h >= len) {
    h = step < 0 ? len - 1 : len;
  }
  int32 n;
  if (step > 0)
    n = l < h ? (h - l + step - 1) / step : 0;
  else
    n = l > h ? (l - h + (-step) - 1) / (-step) : 0;
  if (n < 0) n = 0;
  char *x = _CG_string_alloc(n);
  if (n) {
    if (step == 1)
      memcpy(x, s + l, n);
    else
      for (int32 i = 0; i < n; i++) x[i] = s[l + i * step];
  }
  return x;
}

#ifdef __cplusplus
static inline char *_CG_prim_primitive_to_string(double d) { return _CG_str_from_float(d); }

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

// `sep.join(parts)` for str and bytes, in ONE allocation: `parts` is a
// list of strings/bytes (pointer elements). The __pyc__ join used to
// concatenate pairwise, O(n^2) in the result length; minpng joins 1.9
// million 3-byte pieces.
inline char *_CG_string_join(const char *sep, _CG_list parts) {
  uint32 n = parts ? _CG_list_len(parts) : 0;
  // Elements live at the header's `ptr`, which moves when the list grows.
  char **d = n ? (char **)_CG_list_ptr(parts) : 0;
  size_t ls = _CG_string_len(sep), tot = 0;
  for (uint32 i = 0; i < n; i++) tot += _CG_string_len(d[i]);
  if (n > 1) tot += ls * (n - 1);
  char *x = _CG_string_alloc(tot);
  char *o = x;
  for (uint32 i = 0; i < n; i++) {
    if (i && ls) { memcpy(o, sep, ls); o += ls; }
    size_t l = _CG_string_len(d[i]);
    memcpy(o, d[i], l);
    o += l;
  }
  return x;
}



// issues/044: unlike _CG_list_resize_internal (backing list.append(),
// which correctly mutates in place -- CPython's append() does too),
// this backs list.__add__ (`+`), which CPython never lets mutate
// either operand. The old body wrote the concatenated result into
// l1's own header and returned l1 itself -- any receiver aliased
// elsewhere (an object field, another variable) silently gained the
// concatenation as an in-place side effect of merely evaluating `+`.
// Fresh-allocate the header too, mirroring _CG_list_getslice_internal
// below (which already gets this right for `list[a:b]`).
static inline _CG_list _CG_list_add_internal(_CG_list l1, _CG_list l2, uint32 size1, uint32 size2) {
  uint32 s1 = _CG_prim_len(0, l1), s2 = _CG_prim_len(0, l2);
  uint32 size = size1 ? size1 : size2;
  _CG_list x = _CG_ptr_to_list((_CG_list)MALLOC(size * (s1 + s2) + SIZEOF_LIST_HEADER));
  _CG_list_len(x) = s1 + s2;
  _CG_list_total_len(0, x) = s1 + s2;
  _CG_list_ptr(x) = x;
  if (s1) memcpy(_CG_list_ptr(x), _CG_list_ptr(l1), s1 * size);
  if (s2) memcpy(((char *)_CG_list_ptr(x)) + s1 * size, _CG_list_ptr(l2), s2 * size);
  return x;
}

// list.append() is this function with new_len = len + 1, so it used to
// MALLOC a fresh buffer and memcpy the WHOLE list on every single append
// -- O(n^2) copies and n allocations to build a list of n elements.
// Measured: 200000 appends took CPython 0.01s and pyc more than 9s
// without finishing. shedskin_examples/collatz builds a 131072-entry
// lookup table that way and never got past its own setup.
//
// The header already had the slot for the fix. `total_len` was written
// everywhere and READ nowhere -- always just set equal to `len` -- so it
// is free to mean what its name says: the allocated CAPACITY. Grow it
// geometrically and an append is O(1) amortized. Every other constructor
// in this file (_CG_list_add_internal, _CG_list_setslice_internal,
// _CG_prim_tuple_list_internal, _CG_TUPLE_TO_LIST_FUN) already sets
// total_len to exactly what it allocated, so capacity == total_len is an
// invariant they all satisfy already, including the ones that store
// their elements INLINE in the header.
static inline _CG_list _CG_list_resize_internal(_CG_list l1, uint32 size1, uint32 new_len) {
  uint32 s1 = _CG_prim_len(0, l1);
  uint32 cap = _CG_list_total_len(0, l1);
  if (new_len && new_len <= cap && _CG_list_ptr(l1)) {
    // Already big enough: move the length and zero anything newly
    // exposed. No allocation, no copy -- this is the amortized path.
    if (new_len > s1) memset(((char *)_CG_list_ptr(l1)) + s1 * size1, 0, (new_len - s1) * size1);
    _CG_list_len(l1) = new_len;
    return l1;
  }
  uint32 newcap = cap < 4 ? 4 : cap * 2;
  if (newcap < new_len) newcap = new_len;
  _CG_list x = new_len ? (_CG_list)MALLOC(size1 * newcap) : 0;
  // `y * size1`, not `s1 * size1`: the old code sized the copy by the
  // OLD length while allocating for the new one, so shrinking a list
  // read and wrote past the end of the fresh buffer.
  uint32 y = s1 < new_len ? s1 : new_len;
  if (y) memcpy(x, _CG_list_ptr(l1), y * size1);
  if (new_len > s1) memset(((char *)x) + s1 * size1, 0, (new_len - s1) * size1);
  _CG_list_len(l1) = new_len;
  _CG_list_total_len(0, l1) = new_len ? newcap : 0;
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

// See _CG_string_getslice's comment (same file) for the sentinel
// (INT_MIN/INT_MAX omitted-bound) and negative-step algorithm this
// mirrors -- CPython's PySlice_GetIndicesEx. Also fixes a latent
// signed/unsigned bug in the old clamp (`len` was uint32, so `l >
// len` on a negative int32 `l` promoted `l` to a huge unsigned value
// first, comparing true and clamping `l` to `len` *before* the
// negative-index branch below ever ran -- `a[-3:]` on a 5-element
// list returned `[]` instead of the last 3 elements, confirmed
// separately broken from the missing negative-step support).
static inline _CG_list _CG_list_getslice_internal(_CG_list v, uint32 size, int32 l, int32 h, int32 s) {
  int32 len = (int32)_CG_prim_len(0, v);
  if (!s) s = 1;
  if (l == INT32_MIN) {
    l = s < 0 ? len - 1 : 0;
  } else if (l < 0) {
    l += len;
    if (l < 0) l = s < 0 ? -1 : 0;
  } else if (l >= len) {
    l = s < 0 ? len - 1 : len;
  }
  if (h == INT32_MAX) {
    h = s < 0 ? -1 : len;
  } else if (h < 0) {
    h += len;
    if (h < 0) h = s < 0 ? -1 : 0;
  } else if (h >= len) {
    h = s < 0 ? len - 1 : len;
  }
  int32 n;
  if (s > 0)
    n = l < h ? (h - l + s - 1) / s : 0;
  else
    n = l > h ? (l - h + (-s) - 1) / (-s) : 0;
  if (n < 0) n = 0;
  _CG_list x = _CG_ptr_to_list((_CG_list)MALLOC(size * n + SIZEOF_LIST_HEADER));
  _CG_list_len(x) = n;
  _CG_list_total_len(0, x) = n;
  _CG_list_ptr(x) = x;
  if (n) {
    if (s == 1)
      memcpy(x, ((char *)_CG_list_ptr(v)) + l * size, n * size);
    else
      for (int32 i = 0; i < n; i++) memcpy(((char *)x) + i * size, ((char *)_CG_list_ptr(v)) + (l + (int32)i * s) * size, size);
  }
  return x;
}

// issues/166: an EXTENDED slice store -- `a[i:j:k] = v` with k != 1.
//
// `__pyc_setslice__` has always been handed the step by the frontend and
// always dropped it on the floor, and this function had no parameter to
// receive it, so every strided store was executed as the CONTIGUOUS
// splice `a[i:i+len(v)] = v`. `a[3::4] = [0]*5` on a 20-element list
// replaced elements 3..7 and TRUNCATED the list to 8, silently, exit 0.
// That is `sieve`'s wrong answer: its Sieve of Eratostenes is built on
// `sieve[bottom::si] = [0] * n`, so the whole algorithm collapsed and it
// printed `nprimes: 4` for CPython's 664579.
//
// The read side was already right (`_CG_list_getslice_internal` mirrors
// CPython's PySlice_GetIndicesEx, negative steps included); this is the
// store side catching up, and it reuses that normalisation exactly so the
// two cannot drift.
//
// CPython semantics, and the reason the two branches differ: a CONTIGUOUS
// slice store may resize the list (`a[1:3] = [9]` shortens it), but an
// EXTENDED one may not -- the value length must equal the slice length,
// or it is `ValueError: attempt to assign sequence of size N to extended
// slice of size M`. So the k == 1 path keeps the splice-and-resize code
// below verbatim, and the strided path stores in place.
static inline _CG_list _CG_list_setslice_strided(_CG_list l1, uint32 size, int32 l, int32 h, int32 s,
                                                 _CG_list l2) {
  int32 len1 = (int32)_CG_prim_len(0, l1), len2 = (int32)_CG_prim_len(0, l2);
  // Identical to _CG_list_getslice_internal's normalisation.
  if (l == INT32_MIN) {
    l = s < 0 ? len1 - 1 : 0;
  } else if (l < 0) {
    l += len1;
    if (l < 0) l = s < 0 ? -1 : 0;
  } else if (l >= len1) {
    l = s < 0 ? len1 - 1 : len1;
  }
  if (h == INT32_MAX) {
    h = s < 0 ? -1 : len1;
  } else if (h < 0) {
    h += len1;
    if (h < 0) h = s < 0 ? -1 : 0;
  } else if (h >= len1) {
    h = s < 0 ? len1 - 1 : len1;
  }
  int32 n;
  if (s > 0)
    n = l < h ? (h - l + s - 1) / s : 0;
  else
    n = l > h ? (l - h + (-s) - 1) / (-s) : 0;
  if (n < 0) n = 0;
  // CPython raises ValueError here. pyc has no exception path out of a
  // runtime helper, so this takes cg.cc's `assert(!"runtime error: ...")`
  // convention -- loud, rather than the silent corruption it replaces.
  if (n != len2) {
    assert(!"runtime error: attempt to assign a sequence of the wrong size to an extended slice");
    return l1;
  }
  char *dst = (char *)_CG_list_ptr(l1);
  char *src = (char *)_CG_list_ptr(l2);
  for (int32 i = 0; i < n; i++) memcpy(dst + (size_t)(l + i * s) * size, src + (size_t)i * size, size);
  return l1;
}

// The CONTIGUOUS splice: replace `[l, h)` with `len2` elements read from
// `src`, resizing the list. `src` may be null when `len2` is 0, which is
// how a contiguous DELETE is expressed (issues/166) -- factored out of
// _CG_list_setslice_internal rather than copied into
// _CG_list_delslice_internal so the two cannot drift, the same reason the
// strided paths share _CG_list_getslice_internal's normalisation.
//
// `s` is a byte count here, not a step; the callers use `st` for the step.
static inline _CG_list _CG_list_splice_internal(_CG_list l1, uint32 size, int32 l, int32 h, const void *src,
                                                int32 len2) {
  // SIGNED. These used to be uint32, which made `l > len1` promote a
  // negative bound to a huge unsigned value: the omitted-lower sentinel
  // INT_MIN read as 2147483648, so `del x[:]` clamped l to len1 instead
  // of 0 and deleted NOTHING. (`del x[i:j]` with explicit non-negative
  // bounds was unaffected, which is why it went unnoticed -- and until
  // `del` was lowered at all, nothing reached here.) A negative bound is
  // the whole point of the two `if (... < 0)` branches just below, so
  // the comparison feeding them has to be signed too.
  int32 len1 = (int32)_CG_prim_len(0, l1);
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
    memcpy(p, src, (size_t)len2 * size);
    p += len2 * size;
  }
  int sh = len1 - h;
  if (sh) {
    memcpy(p, ((char *)p1) + h * size, sh * size);
    p += sh * size;
  }
  return l1;
}

// `st` (step), not `s`: the splice above uses `s` for a byte count.
static inline _CG_list _CG_list_setslice_internal(_CG_list l1, uint32 size, int32 l, int32 h, int32 st,
                                                 _CG_list l2) {
  if (!st) st = 1;
  if (st != 1) return _CG_list_setslice_strided(l1, size, l, h, st, l2);
  return _CG_list_splice_internal(l1, size, l, h, _CG_list_ptr(l2), (int32)_CG_prim_len(0, l2));
}

// issues/166: `del a[i:j:k]`. A DIFFERENT operation from the extended
// store `a[i:j:k] = v` next door, which is why it needed its own entry
// point: CPython REMOVES the selected elements and shrinks the list,
// where an extended store may not resize at all and raises ValueError on
// a length mismatch. Lowered here as `__pyc_delslice__`, so the frontend
// no longer has to pin the step to 1 to keep `del a[i:j]` working.
//
// k == 1 is the ordinary contiguous splice with nothing inserted, so it
// goes through the shared helper and behaves exactly as before.
static inline _CG_list _CG_list_delslice_internal(_CG_list l1, uint32 size, int32 l, int32 h, int32 st) {
  if (!st) st = 1;
  if (st == 1) return _CG_list_splice_internal(l1, size, l, h, 0, 0);
  int32 len1 = (int32)_CG_prim_len(0, l1);
  // Identical to _CG_list_getslice_internal's normalisation.
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
  int32 n;
  if (st > 0)
    n = l < h ? (h - l + st - 1) / st : 0;
  else
    n = l > h ? (l - h + (-st) - 1) / (-st) : 0;
  if (n <= 0) return l1;
  // A negative step selects the SAME SET of indices as its positive
  // mirror, just visited in the other order, and deletion does not care
  // about order -- so walk ascending and the compaction below is one pass.
  int32 start = st > 0 ? l : l + (n - 1) * st;
  int32 step = st > 0 ? st : -st;
  char *p = (char *)_CG_list_ptr(l1);
  int32 w = 0, k = 0, next = start;
  for (int32 r = 0; r < len1; r++) {
    if (k < n && r == next) {
      k++;
      next = start + k * step;
      continue;
    }
    if (w != r) memcpy(p + (size_t)w * size, p + (size_t)r * size, size);
    w++;
  }
  // Length shrinks; CAPACITY (total_len) deliberately does not, matching
  // _CG_list_resize_internal's amortised-growth contract -- the buffer
  // stays as big as it was and the next append reuses it.
  _CG_list_len(l1) = w;
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
#define _CG_list_setslice(_l1, _s, _lower, _upper, _step, _l2) \
  (_CG_list_setslice_internal(_l1, _s, _lower, _upper, _step, _CG_to_list(_l2)))
#define _CG_list_delslice(_l1, _s, _lower, _upper, _step) \
  (_CG_list_delslice_internal(_l1, _s, _lower, _upper, _step))
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
// Shallow-copy a GC object whose STATIC type is a union of same-class
// CreationSets (emitted C type _CG_any) -- no compile-time sizeof is
// possible, but Boehm's GC_size gives the allocation's block size at
// runtime, and copying the whole block is exactly the shallow-copy
// semantics P_prim_copy wants (issues/029: __deepcopy__ receivers
// unioning original+copy CSs of one class).
inline void *_CG_prim_copy_any(void *p) {
  if (!p) return p;
  size_t sz = GC_size(p);
  void *x = GC_MALLOC(sz);
  memcpy(x, p, sz);
  return x;
}
#define _CG_prim_clone_vector(_c, _v) _CG_prim_primitive_clone_vector(_c, sizeof(*(_c)), _v)
#define _CG_prim_reply(_s, _c, _r) return _r
#define _CG_prim_primitive(_p, _x) printf("%d\n", (unsigned int)(uintptr_t)_x);
#define _CG_prim_add(_a, _op, _b) ((_a) + (_b))
#define _CG_prim_subtract(_a, _op, _b) ((_a) - (_b))
#define _CG_prim_rsh(_a, _op, _b) ((_a) >> (_b))
// Widen the left operand: when it is a C literal (`7 << 40`, emitted as
// `(7) << (40)`) it is a 32-bit `int`, so the shift is undefined past bit
// 31 -- it printed -3 on one run and 104249815179696 on the next, where
// Python gives 7696581394432. Shifts are the one integer operator FA does
// not constant-fold, so they are the one that reached C with literal
// operands. Python ints are int64 here.
#define _CG_prim_lsh(_a, _op, _b) (((int64)(_a)) << (_b))
#define _CG_prim_mult(_a, _op, _b) ((_a) * (_b))
// found while porting issues/041's colorsys shim: raw C `%` is invalid
// for floating operands (needs fmod), AND -- separately, pre-existing,
// confirmed via `-7 % 3` giving C's -1 instead of Python's 2 -- C's
// (and fmod's) truncated-toward-zero remainder has the wrong sign
// convention vs. Python's floored one (result takes the sign of the
// divisor, not the dividend). Both fixed together since they're the
// same operator: overloaded so int % int stays exact integer
// arithmetic (no float round-trip) while float operands use fmod,
// then both apply the standard truncated-to-floored adjustment.
//
// One template rather than an (int64, int64) / (double, double) overload
// pair: a bare C literal (`20000 % 2`) is `int`, which converts equally
// well to both, and the call was ambiguous. That only surfaces once `%` on
// two literals stops folding in FA -- which it does since ifa/151 stopped
// cloning int arithmetic per constant -- so any integral pair is integer
// arithmetic and anything else is floating.
#ifdef __cplusplus
template <class A, class B>
static inline auto _CG_mod_impl(A a, B b) {
  if constexpr (std::is_integral_v<A> && std::is_integral_v<B>) {
    int64 r = (int64)a % (int64)b;
    if (r != 0 && ((r < 0) != ((int64)b < 0))) r += (int64)b;
    return r;
  } else {
    double r = fmod((double)a, (double)b);
    if (r != 0.0 && ((r < 0.0) != ((double)b < 0.0))) r += (double)b;
    return r;
  }
}
#define _CG_prim_mod(_a, _op, _b) (_CG_mod_impl((_a), (_b)))
#else
#define _CG_prim_mod(_a, _op, _b) ((_a) % (_b))
#endif
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
// ifa/issues/070: str/bytes buffers are length-prefixed (_CG_string_len
// reads the true length from the header), not NUL-terminated -- these used
// to be strcmp()-based, which stops comparing at the first embedded NUL and
// made e.g. b"a\x00b" == b"a\x00c" incorrectly return true. Compare/hash
// exactly `_CG_string_len` bytes instead.
inline _CG_bool _CG_str_eq(const char *a, const char *b) {
  size_t la = _CG_string_len(a), lb = _CG_string_len(b);
  return (_CG_bool)(la == lb && (la == 0 || memcmp(a, b, la) == 0));
}
inline _CG_bool _CG_str_ne(const char *a, const char *b) { return (_CG_bool)!_CG_str_eq(a, b); }
// str.__hash__ (issue 025: hash() builtin). FNV-1a, masked positive:
// deterministic across runs (unlike CPython's SipHash with
// PYTHONHASHSEED randomization) -- programs only rely on
// self-consistency within one run.
inline long long _CG_str_hash(const char *s) {
  size_t len = _CG_string_len(s);
  unsigned long long h = 14695981039346656037ULL;
  for (size_t i = 0; i < len; i++) h = (h ^ (unsigned char)s[i]) * 1099511628211ULL;
  return (long long)(h & 0x7fffffffffffffffULL);
}
inline int _CG_str_cmp(const char *a, const char *b) {
  size_t la = _CG_string_len(a), lb = _CG_string_len(b);
  size_t lm = la < lb ? la : lb;
  int c = lm ? memcmp(a, b, lm) : 0;
  if (c) return c;
  return la < lb ? -1 : (la > lb ? 1 : 0);
}
inline _CG_bool _CG_str_lt(const char *a, const char *b) { return (_CG_bool)(_CG_str_cmp(a, b) < 0); }
inline _CG_bool _CG_str_le(const char *a, const char *b) { return (_CG_bool)(_CG_str_cmp(a, b) <= 0); }
inline _CG_bool _CG_str_gt(const char *a, const char *b) { return (_CG_bool)(_CG_str_cmp(a, b) > 0); }
inline _CG_bool _CG_str_ge(const char *a, const char *b) { return (_CG_bool)(_CG_str_cmp(a, b) >= 0); }
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
