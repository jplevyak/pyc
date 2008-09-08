class int:
#  @must_specialize("x:anynum")
  def __add__(self, x):
    return "#operator"(self, "+", x)
#  def __add__(self, x):
#    return x.__radd__(self)
