@vector("s")
class bytearray:
  length = 0
  def __init__(self, s):
    self.length = s
  def __getitem__(self, key):
    if key < 0:
      key = key + self.length
    return __pyc_primitive__(__pyc_symbol__("coerce"), int,
                             __pyc_primitive__(__pyc_symbol__("index_object"), self, key))
  def __setitem__(self, key, value):
    if key < 0:
      key = key + self.length
    return __pyc_primitive__(__pyc_symbol__("set_index_object"), self,
                             key,
                             __pyc_primitive__(__pyc_symbol__("coerce"), __pyc_char__, value))
  def __len__(self):
    return self.length
  def __pyc_slice_bounds__(self, i, j, s):
    # (start, count) of the slice self[i:j:s], normalised exactly as
    # _CG_list_getslice_internal does (pyc_c_runtime.h). An omitted bound
    # arrives as INT32_MIN / INT32_MAX. list and str slice in C; a
    # bytearray is a record with a vector tail, not a _CG_list, so it
    # slices here.
    n = self.length
    if s == 0:
      raise ValueError("slice step cannot be zero")
    if i == -2147483648:
      i = n - 1 if s < 0 else 0
    elif i < 0:
      i += n
      if i < 0:
        i = -1 if s < 0 else 0
    elif i >= n:
      i = n - 1 if s < 0 else n
    if j == 2147483647:
      j = -1 if s < 0 else n
    elif j < 0:
      j += n
      if j < 0:
        j = -1 if s < 0 else 0
    elif j >= n:
      j = n - 1 if s < 0 else n
    if s > 0:
      c = (j - i + s - 1) // s if i < j else 0
    else:
      c = (i - j - s - 1) // (-s) if j < i else 0
    return (i, c)
  def __pyc_getslice__(self, i, j, s):
    start, c = self.__pyc_slice_bounds__(i, j, s)
    r = bytearray(c)
    for k in range(c):
      r[k] = self[start + k * s]
    return r
  def __pyc_setslice__(self, i, j, s, v):
    # softrender's `self.components[:] = self.reset` (an unresolved
    # __pyc_setslice__ before this existed). A bytearray here has a
    # fixed-size vector (it has no append/extend/insert either), so an
    # assignment that would RESIZE it cannot be represented: that raises,
    # where CPython would resize. For a step other than 1, CPython raises
    # too, with this message.
    start, c = self.__pyc_slice_bounds__(i, j, s)
    m = len(v)
    if m != c:
      raise ValueError("attempt to assign bytes of size " + str(m) + " to extended slice of size " + str(c))
    # Read the source in full first: `a[1:] = a` must see the old values.
    vals = []
    for k in range(m):
      vals.append(v[k])
    for k in range(c):
      self[start + k * s] = vals[k]
  def __pyc_tobytearray__(self):
    # bytearray(a_bytearray): a copy.
    n = self.length
    r = bytearray(n)
    for i in range(n):
      r[i] = self[i]
    return r
  def __pyc_tobytes__(self):
    # bytes(a_bytearray) (sokoban's `bytes(data2)`).
    r = []
    for i in range(self.length):
      r.append(self[i])
    return r.__pyc_tobytes__()
  def __iter__(self):
    return __base_iter__(self)
  def __str__(self):
    x = "bytearray(b'"
    for k in range(0, len(self)):
      c = self[k]
      if c == 9:
        x += "\\t"
      elif c == 10:
        x += "\\n"
      elif c == 13:
        x += "\\r"
      elif c == 92:
        x += "\\\\"
      elif c == 39:
        x += "\\'"
      elif c >= 32 and c < 127:
        x += chr(c)
      else:
        x += "\\x" + __byte_hex2(c)
    x += "')"
    return x
