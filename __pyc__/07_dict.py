class __dict_iter__:
  _keys = []
  _len = 0
  _pos = 0
  def __iter__(self):
    # Iterators are self-iterable (Python protocol) -- lets
    # `for x in it:` consume an already-made iterator (functools
    # .reduce, issue 025).
    return self
  def __init__(self, keys, n):
    self._keys = keys
    self._len = n
    self._pos = 0
  def __pyc_more__(self):
    return self._pos < self._len
  def __next__(self):
    self._pos += 1
    return self._keys[self._pos - 1]
  def __pyc_tolist__(self):
    # `list(d.keys())`/`list(d.values())` (both share this class --
    # plcfrs.py's `list(C.values())`) route through list()'s generic
    # __pyc_tolist__ dispatch (python_ifa_build_if1.cc), which no
    # plain iterator class defined before now -- consumes any
    # remaining items via the existing __pyc_more__/__next__ protocol
    # rather than reading `_keys` directly, so a partially-consumed
    # iterator still yields only what's left (matching real Python).
    r = []
    while self.__pyc_more__():
      r = r.append(self.__next__())
    return r

class __dict_items_iter__:
  _keys = []
  _vals = []
  _len = 0
  _pos = 0
  def __iter__(self):
    return self
  def __init__(self, keys, vals, n):
    self._keys = keys
    self._vals = vals
    self._len = n
    self._pos = 0
  def __pyc_more__(self):
    return self._pos < self._len
  def __next__(self):
    self._pos += 1
    return (self._keys[self._pos - 1], self._vals[self._pos - 1])
  def __pyc_tolist__(self):
    # `list(d.items())` (sunfish.py) -- same rationale as
    # __dict_iter__.__pyc_tolist__ above.
    r = []
    while self.__pyc_more__():
      r = r.append(self.__next__())
    return r

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
  def keys(self):
    # issues/025 "has no type" bucket: dict had no .keys()/.values()/
    # .items() at all (loop, mastermind2, plcfrs, sunfish all hit this
    # exact gap independently). Not a live view (unlike real Python's
    # dict_keys/dict_values/dict_items) -- a fresh snapshot iterator,
    # matching this file's existing __iter__ and __pyc__'s established
    # eager-not-lazy convention (see 08_set.py, genexpr handling);
    # every corpus usage found iterates immediately without mutating
    # the dict mid-iteration, so this is observably identical there.
    return __dict_iter__(self._keys, self._len)
  def values(self):
    return __dict_iter__(self._vals, self._len)
  def items(self):
    return __dict_items_iter__(self._keys, self._vals, self._len)
  def __contains__(self, key):
    i = 0
    while i < self._len:
      if self._keys[i] == key:
        return True
      i += 1
    return False
  def __eq__(self, d):
    if self._len != len(d):
      return False
    i = 0
    while i < self._len:
      k = self._keys[i]
      if not d.__contains__(k):
        return False
      if d[k] != self._vals[i]:
        return False
      i += 1
    return True
  def __ne__(self, d):
    return not self.__eq__(d)
  def __pyc_to_bool__(self):
    return self._len != 0
  def __str__(self):
    x = "{"
    i = 0
    while i < self._len:
      if i:
        x += ", "
      x += self._keys[i].__repr__()
      x += ": "
      x += self._vals[i].__repr__()
      i += 1
    x += "}"
    return x
  def __repr__(self):
    return self.__str__()

# issue 025 "has no type" bucket: dict(iterable_of_pairs) -- same
# shape as set(iterable) in __pyc__/08_set.py: `dict` has no
# __init__ that accepts a value to build from (only the zero-arg
# form). update()'s existing `other` is itself a dict (`for k in
# other: self[k] = other[k]`), which doesn't fit an iterable of
# (key, value) tuples -- e.g. `dict((x, 0.0) for x in AMINOACIDS)`
# (shedskin's adatron.py). A new function, not a dict method, since
# real Python's dict(iterable) form takes 2-tuples, not another
# dict's `__iter__`-over-keys shape update() already relies on.
def __pyc_dict_from_iterable__(pairs):
  d = dict()
  for pair in pairs:
    d[pair[0]] = pair[1]
  return d
