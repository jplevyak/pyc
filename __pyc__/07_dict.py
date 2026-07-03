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
