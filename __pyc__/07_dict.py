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
    self._keys = keys
    self._len = n
    self._pos = 0
  def __pyc_more__(self):
    # ifa/175, for a field-held bound: `_pos` cannot carry an empty
    # dict's fact -- `__next__`'s `+= 1` widens it once the body is live,
    # and the loop feeds itself. `_len` is never written after __init__,
    # and `!=` clones constants, so an empty dict folds to False here.
    return self._len != 0 and self._pos < self._len
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
    self._index = []
    self._mask = 0
  # issues/118: a hash index over insertion-ordered storage -- the same
  # layout as `set` (__pyc__/08_set.py, which explains it): `_keys` /
  # `_vals` in insertion order, `_index` slot -> position + 1, `_mask`
  # 0 until the first insert. The probe is written out per method, never
  # a helper taking `key` (RUNTIME.md): a `_slot(self, key)` helper
  # merged every dict's key type into one contour and miscompiled
  # shedskin_examples/loop (issues/118's write-up).
  def __pyc_rehash__(self):
    # In place, in the container's OWN `_index` list, never a fresh
    # `[0] * cap`: that list would be created inside list.__mul__, one
    # creation point shared with every `[x] * n` in the program (a user's
    # `[0.0] * n` would make the index int|float), and a second list
    # creation point per container that separates a populated dict from
    # an empty one -- exposing the empty one's loop bodies, which FA
    # cannot yet prove dead (ifa/072; `print({})` next to `print(d)`).
    n = len(self._index)
    cap = n
    if cap < 8:
      cap = 8
    while cap < (self._len + 1) * 2 + 1:
      cap = cap * 2
    # Resized with the primitive list.append itself uses, whose result is
    # typed as the receiver's own contour (merge_in). NOT via append():
    # `self._keys`, `self._vals` and `self._index` are all appended to, so
    # when their element types coincide (an int-keyed, int-valued dict)
    # the three calls share one append contour, its return flows back
    # into all three fields, every list reaches every field, and the
    # partition that keeps them apart finds one group. And resize, like
    # append, may REALLOCATE, so its result must be stored.
    idx = self._index
    if cap != n:
      idx = __pyc_c_call__(__pyc_primitive__(__pyc_symbol__("merge_in"), idx, idx),
                           "_CG_list_resize",
                           list, idx,
                           int, __pyc_primitive__(__pyc_symbol__("sizeof_element"), idx),
                           int, cap)
      self._index = idx
    j = 0
    while j < cap:
      idx[j] = 0
      j += 1
    m = cap - 1
    i = 0
    while i < self._len:
      h = self._keys[i].__hash__()
      s = (h ^ (h >> 4) ^ (h >> 11)) & m
      while idx[s]:
        s = (s + 1) & m
      i += 1
      idx[s] = i
    self._mask = m
  def __len__(self):
    return self._len
  def __getitem__(self, key):
    if self._mask:
      h = key.__hash__()
      m = self._mask
      s = (h ^ (h >> 4) ^ (h >> 11)) & m
      p = self._index[s]
      while p:
        if self._keys[p - 1] == key:
          return self._vals[p - 1]
        s = (s + 1) & m
        p = self._index[s]
    return self._vals[0]
  def __setitem__(self, key, value):
    if (self._len + 1) * 2 > self._mask:
      self.__pyc_rehash__()
    h = key.__hash__()
    m = self._mask
    s = (h ^ (h >> 4) ^ (h >> 11)) & m
    p = self._index[s]
    while p:
      if self._keys[p - 1] == key:
        self._vals[p - 1] = value
        return self
      s = (s + 1) & m
      p = self._index[s]
    self._keys = self._keys.append(key)
    self._vals = self._vals.append(value)
    self._len = self._len + 1
    self._index[s] = self._len
    return self
  def __delitem__(self, key):
    if self._mask == 0:
      return None
    h = key.__hash__()
    m = self._mask
    s = (h ^ (h >> 4) ^ (h >> 11)) & m
    p = self._index[s]
    while p:
      if self._keys[p - 1] == key:
        self._keys.__delitem__(p - 1)
        self._vals.__delitem__(p - 1)
        self._len = self._len - 1
        self.__pyc_rehash__()
        return None
      s = (s + 1) & m
      p = self._index[s]
    return None
  def pop(self, key):
    # shedskin_examples/sudoku5 (`cols.append(X.pop(j))`) needed this; the
    # class had none, so the call was unresolved. Removes and returns the
    # value, raising KeyError on a missing key as CPython does -- the
    # same contract str.index keeps with ValueError. The message is
    # repr(key) because that is what CPython's str(KeyError(key)) prints,
    # and pyc's BaseException.args is a str. The two-argument
    # `pop(key, default)` form is not provided.
    if self._mask:
      h = key.__hash__()
      m = self._mask
      s = (h ^ (h >> 4) ^ (h >> 11)) & m
      p = self._index[s]
      while p:
        if self._keys[p - 1] == key:
          v = self._vals[p - 1]
          self._keys.__delitem__(p - 1)
          self._vals.__delitem__(p - 1)
          self._len = self._len - 1
          self.__pyc_rehash__()
          return v
        s = (s + 1) & m
        p = self._index[s]
    raise KeyError(repr(key))
  def setdefault(self, key, default=None):
    if (self._len + 1) * 2 > self._mask:
      self.__pyc_rehash__()
    h = key.__hash__()
    m = self._mask
    s = (h ^ (h >> 4) ^ (h >> 11)) & m
    p = self._index[s]
    while p:
      if self._keys[p - 1] == key:
        return self._vals[p - 1]
      s = (s + 1) & m
      p = self._index[s]
    self._keys = self._keys.append(key)
    self._vals = self._vals.append(default)
    self._len = self._len + 1
    self._index[s] = self._len
    return default

  def get(self, key, default=None):
    if self._mask:
      h = key.__hash__()
      m = self._mask
      s = (h ^ (h >> 4) ^ (h >> 11)) & m
      p = self._index[s]
      while p:
        if self._keys[p - 1] == key:
          return self._vals[p - 1]
        s = (s + 1) & m
        p = self._index[s]
    return default
  def update(self, other):
    if other is None:
      return self
    for k in other:
      self[k] = other[k]
    return self
  def __iter__(self):
    return __dict_iter__(self._keys, self._len)
  def __pyc_tolist__(self):
    # issues/110: `list(d)` and `tuple(d)` iterate a dict's KEYS. Only
    # the two ITERATOR classes above defined __pyc_tolist__, never
    # `dict` itself, so `list(d)` aborted at runtime with "getter not
    # resolved" while `list(d.keys())` worked -- and `tuple(d)` was the
    # last hole in make_seq's iterable surface.
    #
    # Reads `_keys` directly rather than delegating to
    # `self.keys().__pyc_tolist__()`. A freshly built iterator is never
    # partially consumed, so the protocol loop buys nothing here, and
    # the extra call boundary is exactly what costs make_seq its source
    # CreationSets on a churning final pass (see CreationSet::seq_src).
    r = []
    i = 0
    while i < self._len:
      r = r.append(self._keys[i])
      i += 1
    return r
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
    if self._mask == 0:
      return False
    h = key.__hash__()
    m = self._mask
    s = (h ^ (h >> 4) ^ (h >> 11)) & m
    p = self._index[s]
    while p:
      if self._keys[p - 1] == key:
        return True
      s = (s + 1) & m
      p = self._index[s]
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
def __pyc_dict_from_iterable__(d, pairs):
  for pair in pairs:
    d[pair[0]] = pair[1]
  return d
