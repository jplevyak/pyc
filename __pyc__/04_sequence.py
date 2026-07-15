class __list_iter__:
  thelist = None
  position = 0
  def __init__(self, l):
    self.thelist = l
  def __pyc_more__(self):
    return self.position < len(self.thelist)
  def __next__(self):
    self.position += 1
    return self.thelist.__getitem__(self.position-1)

class list:
  def __len__(self):
    return __pyc_primitive__(__pyc_symbol__("len"), self)
  def __getitem__(self, key):
    return __pyc_primitive__(__pyc_symbol__("index_object"), self, __pyc_clone_constants__(key))
  def __pyc_getslice__(self, i, j, s):
    return __pyc_c_call__(__pyc_primitive__(__pyc_symbol__("merge"), self, self),
                          "_CG_list_getslice",
                          list, self,
                          int, __pyc_primitive__(__pyc_symbol__("sizeof_element"), self),
                          int, i,
                          int, j,
                          int, s)
  def __pyc_setslice__(self, i, j, s, v):
    return __pyc_c_call__(__pyc_primitive__(__pyc_symbol__("merge_in"), self, v),
                          "_CG_list_setslice",
                          list, self,
                          int, __pyc_primitive__(__pyc_symbol__("sizeof_element"), self),
                          int, i,
                          int, j,
                          list, v)
  def __setitem__(self, key, value):
    return __pyc_primitive__(__pyc_symbol__("set_index_object"), self,
                             __pyc_clone_constants__(key), value)
  def __delitem__(self, key):
    pass
  def __iter__(self):
    return __list_iter__(self)
  def __contains__(self, item):
    for x in self:
      if x == item:
        return True
    return False
  def __pyc_tolist__(self):
    # list(xs) copies -- see the list() intercept (issue 025).
    r = []
    for x in self:
      r.append(x)
    return r
  def __add__(self, l):
    return __pyc_c_call__(__pyc_primitive__(__pyc_symbol__("merge_in"), self, l),
                          "_CG_list_add",
                          list, self,
                          int, l,
                          int, __pyc_primitive__(__pyc_symbol__("sizeof_element"), self),
                          int, __pyc_primitive__(__pyc_symbol__("sizeof_element"), l))
  def __radd__(self, l):
    pass
  def __iadd__(self, l):
    return __add__(self, l)
  def __mul__(self, n):
    return __pyc_c_call__(__pyc_primitive__(__pyc_symbol__("merge"), self, self),
                          "_CG_list_mult",
                          list, self,
                          int, n,
                          int, __pyc_primitive__(__pyc_symbol__("sizeof_element"), self), ")")
  def __rmul__(self, l):
    pass
  def __imul__(self, l):
    pass
#  @must_specialize("l:list")
  def __eq__(self, l):
    ll = __pyc_clone_constants__(len(l))
    lself = __pyc_clone_constants__(len(self))
    if lself != ll:
      return False
    for i in range(lself):
      if l[i] != self[i]:
        return False
    return True
  def __ne__(self, l):
    return not self.__eq__(l)
  def append(self, x):
    l = __pyc_primitive__(__pyc_symbol__("len"), self)
    tmp = __pyc_c_call__(__pyc_primitive__(__pyc_symbol__("merge_in"), self, self),
                         "_CG_list_resize",
                         list, self,
                         int, __pyc_primitive__(__pyc_symbol__("sizeof_element"), self),
                         int, l + 1)
    tmp.__setitem__(l, x)
    return tmp
  def index(self, index, start=0, end=0):
    pass
  def count(self, l):
    pass
  def reverse(self):
    i = 0
    j = len(self) - 1
    while i < j:
      t = self[i]
      self[i] = self[j]
      self[j] = t
      i += 1
      j -= 1
    return None
  def sort(self, key=None, reverse=False):
    # Stable insertion sort (matches Python's stability guarantee;
    # reverse=True flips the comparison rather than reversing after,
    # which is what Python's stability under reverse means). The
    # key-is-None branches keep each call contour monomorphic via
    # nil narrowing (same pattern as min/max in 05_builtins.py) --
    # `key(x)`'s type never unions with the element type. Issue 025:
    # circle.py's `circles.sort(key=lambda c: c.offset())` was its
    # first blocker.
    # Comparisons use ONLY `<` (CPython's sort contract: elements
    # need just __lt__ -- voronoi2's Site defines __lt__/__eq__ and
    # nothing else, and pyc has no reflected-operator fallback).
    n = len(self)
    i = 1
    while i < n:
      x = self[i]
      j = i - 1
      if key is None:
        if reverse:
          while j >= 0 and self[j] < x:
            self[j + 1] = self[j]
            j = j - 1
        else:
          while j >= 0 and x < self[j]:
            self[j + 1] = self[j]
            j = j - 1
      else:
        kx = key(x)
        if reverse:
          while j >= 0 and key(self[j]) < kx:
            self[j + 1] = self[j]
            j = j - 1
        else:
          while j >= 0 and kx < key(self[j]):
            self[j + 1] = self[j]
            j = j - 1
      self[j + 1] = x
      i = i + 1
    return None
  def __str__(self):
    x = "["
    for k in range(0,len(self)):
      if (k):
        x += ", "
      x += self[k].__repr__()
    x += "]"
    return x
  def __pyc_to_bool__(self):
    return self.__len__() != 0

class __tuple_iter__:
  thetuple = None
  position = 0
  def __init__(self, t):
    self.thetuple = t
  def __pyc_more__(self):
    return self.position < len(self.thetuple)
  def __next__(self):
    self.position += 1
    return self.thetuple.__getitem__(self.position-1)

class tuple:
  def __getitem__(self, key):
    return __pyc_primitive__(__pyc_symbol__("index_object"), self, __pyc_clone_constants__(key))
  def __setitem__(self, key, value):
    return __pyc_primitive__(__pyc_symbol__("set_index_object"), self, __pyc_clone_constants__(key), value)
  def __iter__(self):
    return __tuple_iter__(self)
  def __len__(self):
    return __pyc_clone_constants__(__pyc_primitive__(__pyc_symbol__("len"), self))
  def __pyc_to_bool__(self):
    return self.__len__() != 0
  def __pyc_tuplify__(self):
    return self
  def __eq__(self, t):
    lt = __pyc_clone_constants__(len(t))
    lself = __pyc_clone_constants__(len(self))
    if lself != lt:
      return False
    for i in range(lself):
      if t[i] != self[i]:
        return False
    return True
  def __ne__(self, t):
    return not self.__eq__(t)
  def __lt__(self, t):
    lt = __pyc_clone_constants__(len(t))
    lself = __pyc_clone_constants__(len(self))
    n = lself if lself < lt else lt
    for i in range(n):
      a = self[i]
      b = t[i]
      if a < b:
        return True
      if b < a:
        return False
    return lself < lt
  def __le__(self, t):
    return not t.__lt__(self)
  def __gt__(self, t):
    return t.__lt__(self)
  def __ge__(self, t):
    return not self.__lt__(t)
  def __str__(self):
    n = len(self)
    x = "("
    for k in range(0, n):
      if (k):
        x += ", "
      x += self[k].__repr__()
    if n == 1:
      x += ","
    x += ")"
    return x
