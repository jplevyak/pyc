class object:
  def __null__(self):
    return False

class __None_type__:
  def __nonzero__(self):
    return False
  def __null__(self):
    return True

class int:
#  @must_specialize("x:anynum") -- for dispatching to __radd__
#  def __add__(self, x):
#    return "#operator"(self, "+", x)
#  def __add__(self, x):
#    return x.__radd__(self)
  def __add__(self, x):
    return "#operator"(self, "+", x)
  def __sub__(self, x):
    return "#operator"(self, "-", x)
  def __mul__(self, x):
    return "#operator"(self, "*", x)
  def __div__(self, x):
    return "#operator"(self, "/", x)
  def __mod__(self, x):
    return "#operator"(self, "%", x)
  def __pow__(self, x):
    return "#operator"(self, "**", x)
  def __lshift__(self, x):
    return "#operator"(self, "<<", x)
  def __rshift__(self, x):
    return "#operator"(self, ">>", x)
  def __or__(self, x):
    return "#operator"(self, "|", x)
  def __xor__(self, x):
    return "#operator"(self, "^", x)
  def __and__(self, x):
    return "#operator"(self, "&", x)
  def __floordiv__(self, x):
    return "#operator"(self, "/", x)
  def __iadd__(self, x):
    return "#operator"(self, "+", x)
  def __isub__(self, x):
    return "#operator"(self, "-", x)
  def __imul__(self, x):
    return "#operator"(self, "*", x)
  def __idiv__(self, x):
    return "#operator"(self, "/", x)
  def __imod__(self, x):
    return "#operator"(self, "%", x)
  def __ipow__(self, x):
    return "#operator"(self, "**", x)
  def __ilshift__(self, x):
    return "#operator"(self, "<<", x)
  def __irshift__(self, x):
    return "#operator"(self, ">>", x)
  def __ior__(self, x):
    return "#operator"(self, "|", x)
  def __ixor__(self, x):
    return "#operator"(self, "^", x)
  def __iand__(self, x):
    return "#operator"(self, "&", x)
  def __ifloordiv__(self, x):
    return "#operator"(self, "/", x)
  def __invert__(self):
    return "#operator"("~", self)
  def __pos__(self):
    return "#operator"("+", self)
  def __neg__(self):
    return "#operator"("-", self)
  def __not__(self):
    return "#operator"("!", self)
  def __eq__(self, x):
    return "#operator"(self, "==", x)
  def __ne__(self, x):
    return "#operator"(self, "!=", x)
  def __lt__(self, x):
    return "#operator"(self, "<", x)
  def __le__(self, x):
    return "#operator"(self, "<=", x)
  def __gt__(self, x):
    return "#operator"(self, ">", x)
  def __ge__(self, x):
    return "#operator"(self, ">=", x)
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
    return "#operator"(self, "+", x)
  def __sub__(self, x):
    return "#operator"(self, "-", x)
  def __mul__(self, x):
    return "#operator"(self, "*", x)
  def __div__(self, x):
    return "#operator"(self, "/", x)
  def __mod__(self, x):
    return "#operator"(self, "%", x)
  def __pow__(self, x):
    return "#operator"(self, "**", x)
  def __lshift__(self, x):
    return "#operator"(self, "<<", x)
  def __rshift__(self, x):
    return "#operator"(self, ">>", x)
  def __or__(self, x):
    return "#operator"(self, "|", x)
  def __xor__(self, x):
    return "#operator"(self, "^", x)
  def __and__(self, x):
    return "#operator"(self, "&", x)
  def __floordiv__(self, x):
    return "#operator"(self, "/", x)
  def __invert__(self):
    return "#operator"("~", self)
  def __pos__(self):
    return "#operator"("+", self)
  def __neg__(self):
    return "#operator"("-", self)
  def __not__(self):
    return "#operator"("!", self)
  def __eq__(self, x):
    return "#operator"(self, "==", x)
  def __ne__(self, x):
    return "#operator"(self, "!=", x)
  def __lt__(self, x):
    return "#operator"(self, "<", x)
  def __le__(self, x):
    return "#operator"(self, "<=", x)
  def __gt__(self, x):
    return "#operator"(self, ">", x)
  def __ge__(self, x):
    return "#operator"(self, ">=", x)
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

class range:
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
  
class __list_iter__:
  thelist = None
  position = 0
  def __init__(self, l):
    self.thelist = l
  def next(self):
    if (self.position < self.thelist.len()):
      self.position += 1
      return thelist.__getitem__(self.thelist, position-1)
    else:
      return None

class list:
  len = 0
  def __len__(self):
    return 0
  def __getitem__(self, key):
    return "#primitive"("index_object", self, key);
  def __setitem__(self, key, value):
    return "#primitive"("set_index_object", self, key, value);
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
    return "#primitive"("set_index_object", self, 0, l);
  def index(self, index, start=0, end=0):
    pass
  def count(self, l):
    pass

