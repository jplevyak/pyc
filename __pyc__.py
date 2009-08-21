class object:
  def __null__(self):
    return False
  def __done__(self):
    return False

class __None_type__:
  def __nonzero__(self):
    return False
  def __null__(self):
    return True

class basestring:
  pass

class str(basestring):
  def __add__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("::"), x)
  def __iadd__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("::"), x)

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

class __list_iter__:
  thelist = None
  position = 0
  def __init__(self, l):
    self.thelist = l
  def __done__(self):
    return self.position >= thelist.len 
  def next(self):
    self.position += 1
    return thelist.__getitem__(self.thelist, position-1)

class list:
  len = 0
  def __len__(self):
    return 0
  def __getitem__(self, key):
    return __pyc_primitive__(__pyc_symbol__("index_object"), self, key)
  def __setitem__(self, key, value):
    return __pyc_primitive__(__pyc_symbol__("set_index_object"), self, key, value)
  def __delitem__(self, key):
    pass
  def __iter__(self):
    return __list_iter__(self)
  def __contains__(self, item):
    pass
  def __add__(self, l):
    pass
  def __radd__(self, l):
    pass
  def __iadd__(self, l):
    pass
  def __mul__(self, l):
    pass
  def __rmul__(self, l):
    pass
  def __imul__(self, l):
    pass
  def append(self, l):
    return __pyc_primitive__(__pyc_symbol__("set_index_object"), self, 0, l)
  def index(self, index, start=0, end=0):
    pass
  def count(self, l):
    pass

class __tuple_iter__:
  thetuple = None
  position = 0
  def __init__(self, t):
    self.thetuple = l
  def __done__(self):
    return self.position >= thetuple.len 
  def next(self):
    self.position += 1
    return thetuple.__getitem__(self.thetuple, position-1)

class tuple:
  def __getitem__(self, key):
    return __pyc_primitive__(__pyc_symbol__("index_object"), self, key)
  def __setitem__(self, key, value):
    return __pyc_primitive__(__pyc_symbol__("set_index_object"), self, key, value)
  def __iter__(self):
    return __tuple_iter__(self)

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
    __pyc_primitive__(__pyc_symbol__("exit"), status)

def range(start, end):
  result = []
  while (start < end):
    result += [start]
  return result

class xrange:
  i = 0
  j = 0
  def __init__(this, ai, aj):
    i = ai
    j = aj
    return this
  def __iter__(this):
    return this
  def next(self):
    if (self.i >= self.j):
      return None  
    x = self.i
    self.i += 1
    return x
