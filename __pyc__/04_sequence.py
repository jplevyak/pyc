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
    ll = __pyc_clone_constants__(l.len())
    lself = __pyc_clone_constants__(self.len());
    if lself == 0:
      if ll == 0:
        return True
      else:
        return False
    else:
      if ll == 0:
        return False
      for i in range(lself):
        if l[i] != self[i]:
           return False
      return True
#  def __eq__(self, l):
#    return False
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
