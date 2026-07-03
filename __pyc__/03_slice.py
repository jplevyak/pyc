class __slice_iter__:
  theslice = None
  position = 0
  def __init__(self, s):
    self.theslice = s
  def __pyc_more__(self):
    return self.position < self.theslice.upper
  def __next__(self):
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
