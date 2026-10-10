# File objects: open(), read/readline/readlines/write/close, and line
# iteration (`for line in f`). Backed by the _CG_f* helpers in
# pyc_c_runtime.h; the handle is a FILE* smuggled through an int field.
# A failed open yields handle 0, which the runtime helpers treat as an
# immediately-EOF / ignore-writes stream (no exception model yet,
# issue 011). sys.std{in,out,err} (pyc_lib/sys.py) and input() are
# instances of / built on this class.

class __pyc_file__:
  handle = 0
  # A file is its own iterator, as in CPython (`iter(f) is f`, and
  # `next(f)` reads a line). The for-loop protocol asks __pyc_more__ before
  # each __next__, which needs one line of look-ahead; `peek` holds it, and
  # every read drains it first, so `next(f)`, `for line in f` and
  # `f.readline()` share one position (mwmatching, minilight).
  peeked = False
  peek = ""
  # `f.name`, `f.mode`, `f.encoding`, as CPython, and what its repr shows.
  # The caller computes encoding and the buffer's binary mode: open() for a
  # real file, constants for the std streams. Computing them HERE made every
  # program analyze the locale query and the mode normalization, since every
  # program reaches this class (the unhandled-exception stderr writer):
  # +33 CreationSets on nbody, which opens no file.
  def __init__(self, handle, name="", mode="r", encoding="utf-8", binmode="rb"):
    self.handle = handle
    # `f.closed`, as CPython (rsync's `if datastream.closed`).
    self.closed = False
    self.name = name
    self.mode = mode
    self.encoding = encoding
    # CPython's text file sits on a binary stream, `.buffer` (tarsalzp
    # reads and writes `sys.stdin.buffer` / `sys.stdout.buffer`). Here it is
    # a binary file over the same C stream.
    self.buffer = __pyc_binfile__(handle, name, binmode)
  # CPython: `<_io.TextIOWrapper name='x' mode='r' encoding='UTF-8'>`, and
  # str() is the same (tonyjpegdecoder's `'converted %s' % f`).
  def __repr__(self):
    return "<_io.TextIOWrapper name=" + repr(self.name) + " mode=" + repr(self.mode) + " encoding=" + repr(self.encoding) + ">"
  def __str__(self):
    return self.__repr__()
  def __pyc_take_peek__(self):
    l = self.peek
    self.peeked = False
    self.peek = ""
    return l
  def read(self, size=-1):
    head = ""
    if self.peeked:
      head = self.__pyc_take_peek__()
      if size >= 0:
        if size <= len(head):
          # Unread the rest of the look-ahead line.
          self.peek = head[size:]
          self.peeked = len(self.peek) > 0
          return head[:size]
        size -= len(head)
    if size < 0:
      return head + __pyc_c_call__(str, "_CG_fread_all", int, self.handle)
    return head + __pyc_c_call__(str, "_CG_fread_n", int, self.handle, int, size)
  def readline(self):
    if self.peeked:
      return self.__pyc_take_peek__()
    return __pyc_c_call__(str, "_CG_freadline", int, self.handle)
  def readlines(self):
    r = []
    while True:
      l = self.readline()
      if len(l) == 0:
        break
      r.append(l)
    return r
  def write(self, s):
    __pyc_c_call__(int, "_CG_fwrite_str", int, self.handle, str, s)
    return None
  def flush(self):
    __pyc_c_call__(int, "_CG_fflush", int, self.handle)
    return None
  def seek(self, offset, whence=0):
    # rdb's `iTunesSD.seek(18)`. Raises on failure, as CPython does. The
    # look-ahead line is discarded, as CPython discards its read-ahead.
    self.__pyc_take_peek__()
    r = __pyc_c_call__(int, "_CG_fseek", int, self.handle, int, offset, int, whence)
    if r < 0:
      raise OSError("seek failed")
    return r
  def tell(self):
    # Mid-iteration CPython refuses too; a line of look-ahead counted in
    # characters cannot be turned back into a byte offset.
    # An empty look-ahead is end of file, where CPython allows tell again.
    if self.peeked and len(self.peek) > 0:
      raise OSError("telling position disabled by next() call")
    return __pyc_c_call__(int, "_CG_ftell", int, self.handle)
  def close(self):
    __pyc_c_call__(int, "_CG_fclose", int, self.handle)
    self.handle = 0
    self.closed = True
    return None
  def __iter__(self):
    return self
  def __pyc_more__(self):
    if not self.peeked:
      self.peek = __pyc_c_call__(str, "_CG_freadline", int, self.handle)
      self.peeked = True
    return len(self.peek) > 0
  def __next__(self):
    l = self.readline()
    if len(l) == 0:
      raise StopIteration()
    return l
  # issues/170: a file is its own context manager. __exit__ closes it and
  # returns False, so an exception raised in the `with` body propagates.
  def __enter__(self):
    return self
  def __exit__(self, typ, value, tb):
    self.close()
    return False

# A failed open raises as CPython does: the OSError subclass for errno, and
# the message `[Errno 2] No such file or directory: 'path'`. It used to
# return a 0 handle unchecked, and the first read segfaulted (doom without
# its WAD). The errno values are POSIX's (Linux and macOS agree on these).
#
# The `raise` statements are in open() and open_binary() THEMSELVES, not in
# a shared helper: builtin-module code does not propagate exceptions
# (emit_exc_check, python_ifa_build_if1.cc), so a raise only reaches the
# caller from the builtin function's own body -- the str.index pattern,
# which also marks the function direct_raise so user call sites check.
def __pyc_open_error_message__(path, e):
  return "[Errno " + str(e) + "] " + __pyc_c_call__(str, "_CG_strerror", int, e) + ": " + repr(path)

def open(path, mode="r"):
  h = __pyc_c_call__(int, "_CG_fopen", str, path, str, mode)
  if h == 0:
    e = __pyc_c_call__(int, "_CG_errno")
    if e == 2:
      raise FileNotFoundError(__pyc_open_error_message__(path, e))
    if e == 13 or e == 1:
      raise PermissionError(__pyc_open_error_message__(path, e))
    if e == 21:
      raise IsADirectoryError(__pyc_open_error_message__(path, e))
    raise OSError(__pyc_open_error_message__(path, e))
  return __pyc_file__(h, path, mode, __pyc_c_call__(str, "_CG_locale_encoding"), __pyc_binary_mode__(mode))

# Binary-mode counterpart to __pyc_file__/open() above: read()/readline()/
# readlines() return bytes instead of str. The open(...) builtin-call
# intercept (python_ifa_build_if1.cc) routes a literal 'rb'/'wb'/etc mode
# string here at compile time instead of to open() -- see that intercept's
# comment for why this can't just be a runtime branch inside one shared
# open()/read(). _CG_fread_all/_CG_fread_n/_CG_freadline are reused
# unchanged: the underlying C storage is identical (_CG_string-shaped
# length-prefixed buffer) either way, so this is a pure typing difference,
# not a new runtime code path.
# A binary file's `.mode` as CPython's FileIO reports it, which is
# normalized: x -> 'xb', a -> 'ab', w -> 'wb', r -> 'rb', and with `+`
# 'xb+', 'ab+', or 'rb+' for both r+ and w+. A text file keeps the mode it
# was given; its `.buffer` has this one.
def __pyc_binary_mode__(mode):
  plus = "+" in mode
  if "x" in mode:
    r = "xb"
  elif "a" in mode:
    r = "ab"
  elif "w" in mode and not plus:
    r = "wb"
  else:
    r = "rb"
  if plus:
    r = r + "+"
  return r

class __pyc_binfile__:
  handle = 0
  # A file is its own iterator, as in CPython (`iter(f) is f`, and
  # `next(f)` reads a line). The for-loop protocol asks __pyc_more__ before
  # each __next__, which needs one line of look-ahead; `peek` holds it, and
  # every read drains it first, so `next(f)`, `for line in f` and
  # `f.readline()` share one position (mwmatching, minilight).
  peeked = False
  peek = b""
  def __init__(self, handle, name="", mode="rb"):
    self.handle = handle
    # `f.closed`, as CPython (rsync's `if datastream.closed`).
    self.closed = False
    # Already normalized by the caller (__pyc_binary_mode__), as CPython's
    # FileIO reports it.
    self.name = name
    self.mode = mode
  # CPython's binary file is a BufferedReader, BufferedWriter or (with `+`)
  # BufferedRandom, by mode: `<_io.BufferedReader name='tiger1.jpg'>`.
  def __repr__(self):
    kind = "BufferedReader"
    if "+" in self.mode:
      kind = "BufferedRandom"
    elif "w" in self.mode or "a" in self.mode or "x" in self.mode:
      kind = "BufferedWriter"
    return "<_io." + kind + " name=" + repr(self.name) + ">"
  def __str__(self):
    return self.__repr__()
  def __pyc_take_peek__(self):
    l = self.peek
    self.peeked = False
    self.peek = b""
    return l
  def read(self, size=-1):
    head = b""
    if self.peeked:
      head = self.__pyc_take_peek__()
      if size >= 0:
        if size <= len(head):
          # Unread the rest of the look-ahead line.
          self.peek = head[size:]
          self.peeked = len(self.peek) > 0
          return head[:size]
        size -= len(head)
    if size < 0:
      return head + __pyc_c_call__(bytes, "_CG_fread_all", int, self.handle)
    return head + __pyc_c_call__(bytes, "_CG_fread_n", int, self.handle, int, size)
  def readline(self):
    if self.peeked:
      return self.__pyc_take_peek__()
    return __pyc_c_call__(bytes, "_CG_freadline", int, self.handle)
  def readlines(self):
    r = []
    while True:
      l = self.readline()
      if len(l) == 0:
        break
      r.append(l)
    return r
  def write(self, s):
    __pyc_c_call__(int, "_CG_fwrite_str", int, self.handle, bytes, s)
    return None
  def flush(self):
    __pyc_c_call__(int, "_CG_fflush", int, self.handle)
    return None
  def seek(self, offset, whence=0):
    # rdb's `iTunesSD.seek(18)`. Raises on failure, as CPython does. The
    # look-ahead line is discarded, as CPython discards its read-ahead.
    self.__pyc_take_peek__()
    r = __pyc_c_call__(int, "_CG_fseek", int, self.handle, int, offset, int, whence)
    if r < 0:
      raise OSError("seek failed")
    return r
  def tell(self):
    # Bytes: the look-ahead is exact, so the position is the C stream's
    # minus what is buffered.
    return __pyc_c_call__(int, "_CG_ftell", int, self.handle) - len(self.peek)
  def close(self):
    __pyc_c_call__(int, "_CG_fclose", int, self.handle)
    self.handle = 0
    self.closed = True
    return None
  def __iter__(self):
    return self
  def __pyc_more__(self):
    if not self.peeked:
      self.peek = __pyc_c_call__(bytes, "_CG_freadline", int, self.handle)
      self.peeked = True
    return len(self.peek) > 0
  def __next__(self):
    l = self.readline()
    if len(l) == 0:
      raise StopIteration()
    return l
  # issues/170: a file is its own context manager. __exit__ closes it and
  # returns False, so an exception raised in the `with` body propagates.
  def __enter__(self):
    return self
  def __exit__(self, typ, value, tb):
    self.close()
    return False

def open_binary(path, mode="rb"):
  # Same raises as open() above, repeated for the reason given there.
  h = __pyc_c_call__(int, "_CG_fopen", str, path, str, mode)
  if h == 0:
    e = __pyc_c_call__(int, "_CG_errno")
    if e == 2:
      raise FileNotFoundError(__pyc_open_error_message__(path, e))
    if e == 13 or e == 1:
      raise PermissionError(__pyc_open_error_message__(path, e))
    if e == 21:
      raise IsADirectoryError(__pyc_open_error_message__(path, e))
    raise OSError(__pyc_open_error_message__(path, e))
  return __pyc_binfile__(h, path, __pyc_binary_mode__(mode))

def input(prompt=""):
  if len(prompt) > 0:
    __pyc_c_call__(int, "_CG_fwrite_str", int, __pyc_c_call__(int, "_CG_fstd", int, 1), str, prompt)
    __pyc_c_call__(int, "_CG_fflush", int, __pyc_c_call__(int, "_CG_fstd", int, 1))
  l = __pyc_c_call__(str, "_CG_freadline", int, __pyc_c_call__(int, "_CG_fstd", int, 0))
  n = len(l)
  if n > 0 and l[n - 1] == "\n":
    return l.__pyc_getslice__(0, n - 1, 1)
  return l
