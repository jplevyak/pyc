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
extern double _CG_str_to_float64(char *s);
extern int64 _CG_str_to_int64(char *s);
extern int64 _CG_str_to_int64_base(char *s, int base);
extern char *_CG_group_digits(const char *core, char sep);
extern char *_CG_pad_align(const char *core, int width, char align, char fill, int sign_len);
extern void  _CG_parse_format_spec(const char *spec, _CG_FormatSpec *out);
extern char *_CG_format_int_spec(int64 val, const char *spec_str);
extern char *_CG_format_float_spec(double val, const char *spec_str);
extern char *_CG_format_str_spec(const char *val, const char *spec_str);
extern char *_CG_string_mult(char *str, int64 n);
extern char *_CG_string_identity(char *s);
extern void *_CG_prim_primitive_clone_vector(void *p, size_t s, size_t v);
extern char *_CG_strcat(const char *a, const char *b);
extern char *_CG_char_from_string(void *s, int i);
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

/* Real process argv storage -- definition (extern-declared in
 * pyc_c_runtime.h), same pattern as _CG_ready_queue_head below: one
 * copy shared by both backends, set once from generated main() via
 * _CG_set_argv before anything else runs. */
int64 _cg_argc = 0;
char **_cg_argv = 0;

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
#undef _CG_list_setslice

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

char* _CG_net_read_str(int fd, int max_len) {
  char* buf = (char*)GC_MALLOC_ATOMIC(max_len + 1);
  int n = read(fd, buf, max_len);
  if (n < 0) n = 0;
  buf[n] = '\0';
  return _CG_String(buf);
}

int _CG_net_write_str(int fd, const char* str) {
  printf("_CG_net_write_str called with fd=%d\n", fd);
  int err = 0;
  socklen_t len = sizeof(err);
  getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
  if (err != 0) {
    printf("socket error before write: %s\n", strerror(err));
    return -1;
  }
  int res = write(fd, str, strlen(str));
  if (res < 0) {
    printf("write failed: %s\n", strerror(errno));
  }
  return res;
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

/* issues/014: the generator's `return value` (StopIteration.value),
 * readable once the generator is exhausted (st->done) -- set by
 * cg_emit_llvm.cc's is_generator P_prim_reply epilogue.
 * __pyc_generator__.__next__()/.send() (09_generator.py) call this
 * exactly once, when advancing reports has_next == False. */
long long _CG_generator_return_value(long long raw_handle) {
  _CG_generator_state *st = (_CG_generator_state *)(intptr_t)raw_handle;
  if (!st) return 0;
  return st->retval;
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
