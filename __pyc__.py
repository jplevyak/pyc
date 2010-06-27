class __pyc_any_type__:
  def __str__(self):
    return "<instance>"
  def __pyc_tuplify__(self):
    return __pyc_primitive__(__pyc_symbol__("make_tuple"), self)
  def __getslice__(self, i, j):
    return self.__getitem__(slice(i,j,1))

class object:
  def __null__(self):
    return False
  def __pyc_more__(self):
    return False
  def __str__(self):
    return "<object>"
  def __nonzero__(self):
    return True
  def __len__(self):
    return 1
  def __pyc_to_bool__(self):
    return __pyc_operator__(__pyc_operator__(__pyc_symbol__("!"), self.__nonzero__().__pyc_to_bool__()),
                            __pyc_symbol__("&&"),
                            self.__len__() != 0)

class __pyc_None_type__:
  def __nonzero__(self):
    return False
  def __null__(self):
    return True
  def __str__(self):
    return "None"
  def __pyc_to_bool__(self):
    return False

class bool:
  def __and__(self, x):
    if (self):
      return x
    else:
      return self
  def __or__(self, x):
    if (self):
      return self
    else:
      return x
  def __not__(self):
    if (self):
      return False
    else:
      return True
  def __str__(self):
    if (self):
      return "True"
    else:
      return "False"
  def __pyc_to_bool__(self):
    return self

class basestring:
  pass

class __str_iter__:
  thestr = None
  position = 0
  slen = 0
  def __init__(self, s):
    self.thestr = s
    self.slen = len(s)
  def __pyc_more__(self):
    return self.position < self.slen
  def next(self):
    self.position += 1
    return self.thestr.__getitem__(self.position-1)

class str(basestring):
  def __add__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("::"), x)
  def __iadd__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("::"), x)
  def __str__(self):
    return __pyc_clone_constants__(self)
  def __getitem__(self, key):
    return __pyc_primitive__(__pyc_symbol__("index_object"), self, key)
  def __setitem__(self, key, value):
    return __pyc_primitive__(__pyc_symbol__("set_index_object"), self, key, value)
  def __len__(self):
    return __pyc_primitive__(__pyc_symbol__("len"), self)
  def __iter__(self):
    return __str_iter__(self)
  def __mod__(self, t):
    return __pyc_primitive__(__pyc_symbol__("__pyc_format_string__"), self, t)

class int:
#  @must_specialize("x:anynum") -- for dispatching to __radd__
#  def __add__(self, x):
#    return __pyc_operator__(self, "+", x)
#  def __add__(self, x):
#    return x.__radd__(self)
  def __add__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("+"), x)
  def __sub__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("-"), x)
  def __mul__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("*"), x)
  def __div__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("/"), x)
  def __mod__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("%"), x)
  def __pow__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("**"), x)
  def __lshift__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("<<"), x)
  def __rshift__(self, x):
    return __pyc_operator__(self, __pyc_symbol__(">>"), x)
  def __or__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("|"), x)
  def __xor__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("^"), x)
  def __and__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("&"), x)
  def __floordiv__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("/"), x)
  def __iadd__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("+"), x)
  def __isub__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("-"), x)
  def __imul__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("*"), x)
  def __idiv__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("/"), x)
  def __imod__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("%"), x)
  def __ipow__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("**"), x)
  def __ilshift__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("<<"), x)
  def __irshift__(self, x):
    return __pyc_operator__(self, __pyc_symbol__(">>"), x)
  def __ior__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("|"), x)
  def __ixor__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("^"), x)
  def __iand__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("&"), x)
  def __ifloordiv__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("/"), x)
  def __invert__(self):
    return __pyc_operator__(__pyc_symbol__("~"), self)
  def __pos__(self):
    return __pyc_operator__(__pyc_symbol__("+"), self)
  def __neg__(self):
    return __pyc_operator__(__pyc_symbol__("-"), self)
  def __not__(self):
    return __pyc_operator__(__pyc_symbol__("!"), self)
  def __eq__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("=="), x)
  def __ne__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("!="), x)
  def __lt__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("<"), x)
  def __le__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("<="), x)
  def __gt__(self, x):
    return __pyc_operator__(self, __pyc_symbol__(">"), x)
  def __ge__(self, x):
    return __pyc_operator__(self, __pyc_symbol__(">="), x)
  def __hash__(self):
    return self
  def __cmp__(self, x):
    if (self < x):
      return -1
    else:
      if (self > x):
        return 1
      else:
        return 0
  def __nonzero__(self):
     return self != 0
  def __null__(self):
     return False
  def __str__(self):
    return __pyc_primitive__(__pyc_symbol__("to_string"), self)
  def __pyc_to_bool__(self):
    return self != 0

class float:
  def __add__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("+"), x)
  def __sub__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("-"), x)
  def __mul__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("*"), x)
  def __div__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("/"), x)
  def __mod__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("%"), x)
  def __pow__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("**"), x)
  def __floordiv__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("/"), x)
  def __iadd__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("+"), x)
  def __isub__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("-"), x)
  def __imul__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("*"), x)
  def __idiv__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("/"), x)
  def __imod__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("%"), x)
  def __ipow__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("**"), x)
  def __ifloordiv__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("/"), x)
  def __invert__(self):
    return __pyc_operator__(__pyc_symbol__("~"), self)
  def __pos__(self):
    return __pyc_operator__(__pyc_symbol__("+"), self)
  def __neg__(self):
    return __pyc_operator__(__pyc_symbol__("-"), self)
  def __not__(self):
    return __pyc_operator__(__pyc_symbol__("!"), self)
  def __eq__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("=="), x)
  def __ne__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("!="), x)
  def __lt__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("<"), x)
  def __le__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("<="), x)
  def __gt__(self, x):
    return __pyc_operator__(self, __pyc_symbol__(">"), x)
  def __ge__(self, x):
    return __pyc_operator__(self, __pyc_symbol__(">="), x)
  def __cmp__(self, x):
    if (self < x):
      return -1
    else: 
      if (self > x):
        return 1
      else:
        return 0
  def __nonzero__(self):
    return self != 0.0
  def __null__(self):
    return False
  def __str__(self):
    return __pyc_primitive__(__pyc_symbol__("to_string"), self)
  def __pyc_to_bool__(self):
    return self != 0.0

class __slice_iter__:
  theslice = None
  position = 0
  def __init__(self, s):
    self.theslice = s
  def __pyc_more__(self):
    return self.position < self.theslice.upper
  def next(self):
    self.position += self.step
    return self.position - 1

class slice:
  lower = 0
  upper = 0
  step = 1
  def __init__(self, alower, anupper, astep):
    self.lower = alower
    self.upper = anupper
    self.step = astep
  def __iter__(self):
    return __tuple_iter__(self)

class __list_iter__:
  thelist = None
  position = 0
  def __init__(self, l):
    self.thelist = l
  def __pyc_more__(self):
    return self.position < len(self.thelist)
  def next(self):
    self.position += 1
    return self.thelist.__getitem__(self.position-1)

class list:
  def __len__(self):
    return __pyc_primitive__(__pyc_symbol__("len"), self)
  def __getitem__(self, key):
    return __pyc_primitive__(__pyc_symbol__("index_object"), self, key)
  def __getslice__(self, i, j):
    return __pyc_c_code__(__pyc_primitive__(__pyc_symbol__("merge"), self, self), 
                          "_CG_list_getslice", 
                          list, self, 
                          int, __pyc_primitive__(__pyc_symbol__("sizeof_element"), self),
                          int, i,
                          int, j)
  def __setitem__(self, key, value):
    return __pyc_primitive__(__pyc_symbol__("set_index_object"), self, key, value)
  def __setslice__(self, i, j, s):
    return __pyc_c_code__(__pyc_primitive__(__pyc_symbol__("merge_in"), self, s), 
                          "_CG_list_setslice", 
                          list, self, 
                          int, __pyc_primitive__(__pyc_symbol__("sizeof_element"), self),
                          int, i,
                          int, j,
                          list, s)
  def __delitem__(self, key):
    pass
  def __iter__(self):
    return __list_iter__(self)
  def __contains__(self, item):
    pass
  def __add__(self, l):
    return __pyc_c_code__(__pyc_primitive__(__pyc_symbol__("merge"), self, l), 
                          "_CG_list_add", 
                          list, self, 
                          int, l, 
                          int, __pyc_primitive__(__pyc_symbol__("sizeof_element"), self),
                          int, __pyc_primitive__(__pyc_symbol__("sizeof_element"), l))
  def __radd__(self, l):
    pass
  def __iadd__(self, l):
    pass
  def __mul__(self, l):
    return __pyc_c_code__(__pyc_primitive__(__pyc_symbol__("merge"), self, l), 
                          "_CG_list_mult", 
                          list, self, 
                          int, l, 
                          int, __pyc_primitive__(__pyc_symbol__("sizeof_element"), self), ")")
  def __rmul__(self, l):
    pass
  def __imul__(self, l):
    pass
  def append(self, l):
    return __add__(self, l)
  def index(self, index, start=0, end=0):
    pass
  def count(self, l):
    pass
  def __str__(self):
    x = "["
    for k in xrange(0,len(self)):
      if (k):
        x += ", "
      x += self[k].__str__()
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
  def next(self):
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

def abs(x):
  if (x < 0):
    return -x
  return x

def all(iterable):
  for element in iterable:
    if not element:
      return False
  return True

def any(iterable):
  for element in iterable:
    if element:
      return True
  return False

def bin(x): # return integer as string in binary
  if x <= 0:
    return "0"
  else:
    if (x&1 == 0):
      s = "0"
    else:
      s = "1"
    x = x >> 1
    while (x > 0):
      if (x&1 == 0):
        s = "0" + s
      else:
        s = "1" + s
      x = x >> 1
    return s

def exit(status = 0):
    __pyc_c_code__(int, "::exit", int, status)

def range(start, end):
  result = []
  while (start < end):
    result += [start]
  return result

class xrange:
  i = 0
  j = 0
  s = 1
  def __init__(self, ai, aj):
    self.i = ai
    self.j = aj
    return self
  def __pyc_more__(self):
    return self.i < self.j
  def __iter__(this):
    return this
  def next(self):
    x = self.i
    self.i += self.s
    return x

def len(x):
  return x.__len__()
