class __dict_iter__:
  _keys = []
  _len = 0
  _pos = 0
  def __init__(self, keys, n):
    self._keys = keys
    self._len = n
    self._pos = 0
  def __pyc_more__(self):
    return self._pos < self._len
  def __next__(self):
    self._pos += 1
    return self._keys[self._pos - 1]

class dict:
  _keys = []
  _vals = []
  _len = 0
  def __init__(self):
    # issues/017: without this, _keys/_vals/_len stay bare class-body
    # attributes -- shared (via the prototype-clone instantiation model)
    # across every dict instance until each one's first write, exactly
    # like Python's classic mutable-class-attribute footgun. A second
    # instance constructed after a first one has already been written to
    # silently reads/writes the wrong data (see issues/017 for the full
    # trace). __new__() already calls __init__ fresh per instance
    # (mirroring __dict_iter__'s own __init__ above, which has the same
    # class-body-default-immediately-overwritten shape); giving each
    # instance its own list objects here, rather than relying on
    # list.append()'s return value to implicitly decouple from the
    # shared default on first write, closes the gap.
    self._keys = []
    self._vals = []
    self._len = 0
  def __len__(self):
    return self._len
  def __getitem__(self, key):
    i = 0
    while i < self._len:
      if self._keys[i] == key:
        return self._vals[i]
      i += 1
    return self._vals[0]
  def __setitem__(self, key, value):
    i = 0
    while i < self._len:
      if self._keys[i] == key:
        self._vals[i] = value
        return self
      i += 1
    self._keys = self._keys.append(key)
    self._vals = self._vals.append(value)
    self._len = self._len + 1
    return self
  def get(self, key, default=None):
    i = 0
    while i < self._len:
      if self._keys[i] == key:
        return self._vals[i]
      i += 1
    return default
  def update(self, other):
    for k in other:
      self[k] = other[k]
    return self
  def __iter__(self):
    return __dict_iter__(self._keys, self._len)
  def __contains__(self, key):
    i = 0
    while i < self._len:
      if self._keys[i] == key:
        return True
      i += 1
    return False
  def __pyc_to_bool__(self):
    return self._len != 0
