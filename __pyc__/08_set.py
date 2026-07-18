class __set_iter__:
  _items = []
  _len = 0
  _pos = 0
  def __iter__(self):
    # Iterators are self-iterable (Python protocol) -- lets
    # `for x in it:` consume an already-made iterator (functools
    # .reduce, issue 025).
    return self
  def __init__(self, items, n):
    self._items = items
    self._len = n
    self._pos = 0
  def __pyc_more__(self):
    return self._pos < self._len
  def __next__(self):
    self._pos += 1
    return self._items[self._pos - 1]

class set:
  _items = []
  _len = 0
  def __init__(self):
    # issues/017: see dict.__init__'s comment in __pyc__/07_dict.py --
    # without this, a second set instance constructed after a first one
    # has already been mutated silently aliases the wrong data.
    self._items = []
    self._len = 0
  def __len__(self):
    return self._len
  def __contains__(self, item):
    i = 0
    while i < self._len:
      if self._items[i] == item:
        return True
      i += 1
    return False
  def add(self, item):
    if not self.__contains__(item):
      self._items = self._items.append(item)
      self._len = self._len + 1
    return self
  def discard(self, item):
    i = 0
    while i < self._len:
      if self._items[i] == item:
        j = i
        while j < self._len - 1:
          self._items[j] = self._items[j + 1]
          j += 1
        self._len = self._len - 1
        return self
      i += 1
    return self
  def remove(self, item):
    # Real Python raises KeyError if `item` isn't present; pyc has no
    # exception support yet (issue 011), so this quietly no-ops on a
    # missing item, matching the rest of __pyc__'s existing convention
    # for "would raise, but exceptions aren't implemented" (e.g.
    # dict.__getitem__ on a missing key).
    return self.discard(item)
  def pop(self):
    item = self._items[0]
    self.discard(item)
    return item
  def clear(self):
    self._items = []
    self._len = 0
    return self
  def __iter__(self):
    return __set_iter__(self._items, self._len)
  def __pyc_to_bool__(self):
    return self._len != 0
  def __eq__(self, other):
    if self._len != other._len:
      return False
    for item in self:
      if not other.__contains__(item):
        return False
    return True
  def __ne__(self, other):
    return not self.__eq__(other)
  def update(self, other):
    for item in other:
      self.add(item)
    return self
  def __str__(self):
    x = "{"
    i = 0
    while i < self._len:
      if i:
        x += ", "
      x += self._items[i].__repr__()
      i += 1
    x += "}"
    return x

# issue 025 "has no type" bucket: set(iterable) -- like list(iterable)
# (04_sequence.py) and str(x) (01_str.py), `set` has no __init__ that
# accepts a value to build from (only the zero-arg form, __init__(self),
# which the generic Type_RECORD constructor lowering already handles
# fine -- see python_ifa_build_if1.cc's issues/022 comment). A 1-arg
# call fell through to that same zero-arg path and silently dropped
# its argument, degrading `set([...])`'s result to a bottom/NOTYPE
# value that cascaded into "illegal call argument type 'set'" and
# (downstream, once fruntime_errors' default NOTYPE-to-void salvage
# kicks in) invalid generated C. python_ifa_build_if1.cc's
# build_builtin_call_pyda dispatches set(iterable) here directly,
# mirroring the list(iterable)/str(x) 1-arg intercepts.
def __pyc_set_from_iterable__(other):
  s = set()
  s.update(other)
  return s
