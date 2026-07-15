# File objects: open(), read/readline/readlines/write/close, and line
# iteration (`for line in f`). Backed by the _CG_f* helpers in
# pyc_c_runtime.h; the handle is a FILE* smuggled through an int field.
# A failed open yields handle 0, which the runtime helpers treat as an
# immediately-EOF / ignore-writes stream (no exception model yet,
# issue 011). sys.std{in,out,err} (pyc_lib/sys.py) and input() are
# instances of / built on this class.

class __pyc_file__:
  handle = 0
  def __init__(self, handle):
    self.handle = handle
  def read(self, size=-1):
    if size < 0:
      return __pyc_c_call__(str, "_CG_fread_all", int, self.handle)
    return __pyc_c_call__(str, "_CG_fread_n", int, self.handle, int, size)
  def readline(self):
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
  def close(self):
    __pyc_c_call__(int, "_CG_fclose", int, self.handle)
    self.handle = 0
    return None
  def __iter__(self):
    return __file_iter__(self)

# Line iterator. The for-loop protocol checks __pyc_more__ before each
# __next__, so the next line is read eagerly and buffered one ahead.
class __file_iter__:
  thefile = None
  nextline = ""
  def __iter__(self):
    # Iterators are self-iterable (Python protocol) -- lets
    # `for x in it:` consume an already-made iterator (functools
    # .reduce, issue 025).
    return self
  def __init__(self, f):
    self.thefile = f
    self.nextline = f.readline()
  def __pyc_more__(self):
    return len(self.nextline) > 0
  def __next__(self):
    l = self.nextline
    self.nextline = self.thefile.readline()
    return l

def open(path, mode="r"):
  return __pyc_file__(__pyc_c_call__(int, "_CG_fopen", str, path, str, mode))

def input(prompt=""):
  if len(prompt) > 0:
    __pyc_c_call__(int, "_CG_fwrite_str", int, __pyc_c_call__(int, "_CG_fstd", int, 1), str, prompt)
    __pyc_c_call__(int, "_CG_fflush", int, __pyc_c_call__(int, "_CG_fstd", int, 1))
  l = __pyc_c_call__(str, "_CG_freadline", int, __pyc_c_call__(int, "_CG_fstd", int, 0))
  n = len(l)
  if n > 0 and l[n - 1] == "\n":
    return l.__pyc_getslice__(0, n - 1, 1)
  return l
