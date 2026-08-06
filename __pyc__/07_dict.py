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
    # ifa/issues/045 (same lever __list_iter__/range already use,
    # __pyc__/04_sequence.py and 05_builtins.py): this class is
    # SHARED program-wide -- every dict's .keys()/.values() call
    # constructs one, so `_keys`'s field type is inherently the union
    # of every calling dict's key/value type, unless something splits
    # them apart. __pyc_clone_constants__ on the ctor param puts this
    # class on the clone_methods_per_cs track (gen_class_pyda): each
    # creating contour gets its OWN iterator CS, and
    # __pyc_more__/__next__/__contains__ split per receiver CS too --
    # without it, self.mapSocks.keys() (int-keyed) and headers.keys()
    # (str-keyed) share one CreationSet whose _keys unions int64 and
    # str across BOTH, unrelated dicts (found via
    # shedskin_examples/webserver/webserver.py: removing the class-
    # body defaults below, mirroring issue 076's dict/set fix, was
    # tried first and made this WORSE -- that fix's premise doesn't
    # hold here, since this union is a genuine cross-instance one, not
    # a same-instance class-body-vs-__init__ artifact).
    self._keys = __pyc_clone_constants__(keys)
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
  def __contains__(self, key):
    # `x in d.keys()` / `x in d.values()`: python_ifa_build_if1.cc
    # lowers `in` unconditionally to a direct __contains__ dispatch on
    # the right operand (no fallback to the general iterable
    # protocol when __contains__ is absent), and this class had none
    # -- the dispatch could never resolve, degrading to "no type"
    # (webserver.py's `s in self.mapSocks.keys()`). Linear scan,
    # matching dict.__contains__'s own style below -- this is a
    # snapshot, not CPython's O(1) hash-backed view.
    i = 0
    while i < self._len:
      if self._keys[i] == key:
        return True
      i += 1
    return False

class __dict_items_iter__:
  _keys = []
  _vals = []
  _len = 0
  _pos = 0
  def __iter__(self):
    return self
  def __init__(self, keys, vals, n):
    # ifa/issues/045: same lever, same rationale as __dict_iter__'s
    # own __init__ above (this class is shared across every dict's
    # .items() call the same way).
    self._keys = __pyc_clone_constants__(keys)
    self._vals = __pyc_clone_constants__(vals)
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
  def __contains__(self, item):
    # `(k, v) in d.items()` -- same gap/rationale as
    # __dict_iter__.__contains__ above.
    i = 0
    while i < self._len:
      if self._keys[i] == item[0] and self._vals[i] == item[1]:
        return True
      i += 1
    return False

class dict:
  def __init__(self):
    # issues/017: without this, _keys/_vals/_len would need to be bare
    # class-body attributes -- shared (via the prototype-clone
    # instantiation model) across every dict instance until each one's
    # first write, exactly like Python's classic mutable-class-attribute
    # footgun. __new__() already calls __init__ fresh per instance, so
    # giving each instance its own list objects here, rather than a
    # class-body default, closes that gap.
    #
    # ifa/issues/076: _keys/_vals/_len are deliberately NOT also
    # declared as class-body defaults (`_keys = []` above the
    # constructor, as this class and 08_set.py's `set` both used to
    # have). Runtime correctness didn't depend on that pair existing --
    # __init__ always overwrites it before any instance is observable --
    # but pyc's flow analysis models a field's type as the UNION of every
    # setter that can reach it, not a temporal overwrite; a bare
    # class-body default is itself a setter (the prototype-clone step
    # copies it into every new instance before __init__ runs), so the
    # class-level default's type NEVER left the field's inferred type
    # even after __init__'s own fresh assignment landed. Confirmed root
    # cause of two dict literals with different key types
    # (`{1:1}`/`{"a":1}`) merging int/str into one union and hard-failing
    # the C build -- removing the redundant class-body defaults here
    # (keeping only __init__'s assignment, which was already the actual
    # fix for issue 017's runtime bug) resolves it. Corpus effect
    # verified net-positive, including recovering dijkstra2 (issue 075's
    # own target) from FAIL to compiling; see that issue's doc for the
    # one accepted trade-off (sudoku2, an already-fragile, unrelated
    # program shifted by the changed convergence timing).
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
    if other is None:
      return self
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
