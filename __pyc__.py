class int:
#  @must_specialize("x:anynum")
  def __add__(self, x):
    return "#operator"(self, "+", x)
#  def __add__(self, x):
#    return x.__radd__(self)
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

class list:
  def append(self, l):
    pass
  def index(self, index, start=0, end=0):
    pass

