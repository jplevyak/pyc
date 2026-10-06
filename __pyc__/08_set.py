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
    # ifa/issues/045 (same lever __list_iter__/range and
    # __dict_iter__ use, __pyc__/04_sequence.py/05_builtins.py/
    # 07_dict.py): this class is shared program-wide -- every set's
    # iteration (__iter__, __pyc_tolist__ via list(s), etc.)
    # constructs one, so `_items`'s field type is inherently the
    # union of every calling set's element type unless something
    # splits them apart. __pyc_clone_constants__ on the ctor param
    # puts this class on the clone_methods_per_cs track
    # (gen_class_pyda): each creating contour gets its OWN iterator
    # CS, and __pyc_more__/__next__ split per receiver CS too.
    self._items = items
    self._len = n
    self._pos = 0
  def __pyc_more__(self):
    # ifa/175, as __dict_iter__: `_len` is never written after __init__,
    # and `!=` clones constants, so an empty set folds to False here even
    # though `_pos += 1` widens `_pos` once the body is live.
    return self._len != 0 and self._pos < self._len
  def __next__(self):
    self._pos += 1
    return self._items[self._pos - 1]

class set:
  # issues/118: a hash index over insertion-ordered storage. `_items`
  # holds the elements in insertion order (iteration, printing and
  # pop() see that order, as before); `_index` is an open-addressed
  # table, slot -> position + 1 (0 = empty), sized to a power of two and
  # kept under half full; `_mask` is its size - 1, and 0 means no index
  # has been built yet. The linear scan this replaces made every
  # membership test O(n) and building a set O(n^2).
  #
  # The probe is written out in every method that takes an element,
  # never factored into a helper taking one: a helper's parameter is one
  # contour shared by every set in the program and merges their element
  # types (RUNTIME.md, "Do not add a shared helper method to a builtin
  # container class"). __pyc_rehash__ takes only self, so it merges
  # nothing the other methods do not.
  def __init__(self):
    # issues/017: see dict.__init__'s comment in __pyc__/07_dict.py --
    # without this, a second set instance constructed after a first one
    # has already been mutated silently aliases the wrong data.
    self._items = []
    self._len = 0
    self._index = []
    self._mask = 0
  def __pyc_rehash__(self):
    # Rebuild the index for the current `_items`, sized for one more
    # insert. Also the whole of deletion: removing an element shifts
    # every later position, so the index is rebuilt rather than patched.
    #
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
      h = self._items[i].__hash__()
      s = (h ^ (h >> 4) ^ (h >> 11)) & m
      while idx[s]:
        s = (s + 1) & m
      i += 1
      idx[s] = i
    self._mask = m
  def __len__(self):
    return self._len
  def __contains__(self, item):
    if self._mask == 0:
      return False
    h = item.__hash__()
    m = self._mask
    s = (h ^ (h >> 4) ^ (h >> 11)) & m
    p = self._index[s]
    while p:
      if self._items[p - 1] == item:
        return True
      s = (s + 1) & m
      p = self._index[s]
    return False
  def add(self, item):
    if (self._len + 1) * 2 > self._mask:
      self.__pyc_rehash__()
    h = item.__hash__()
    m = self._mask
    s = (h ^ (h >> 4) ^ (h >> 11)) & m
    p = self._index[s]
    while p:
      if self._items[p - 1] == item:
        return self
      s = (s + 1) & m
      p = self._index[s]
    self._items = self._items.append(item)
    self._len = self._len + 1
    self._index[s] = self._len
    return self
  def discard(self, item):
    if self._mask == 0:
      return self
    h = item.__hash__()
    m = self._mask
    s = (h ^ (h >> 4) ^ (h >> 11)) & m
    p = self._index[s]
    while p:
      if self._items[p - 1] == item:
        # A real delete. The old shift-down left the last element
        # duplicated past `_len`, so the next add() appended behind it
        # and was lost: {1, 2, 3} -> discard(1) -> add(9) gave
        # `9 in s` False.
        self._items.__delitem__(p - 1)
        self._len = self._len - 1
        self.__pyc_rehash__()
        return self
      s = (s + 1) & m
      p = self._index[s]
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
    self._items.__delitem__(0)
    self._len = self._len - 1
    self.__pyc_rehash__()
    return item
  def clear(self):
    self._items = []
    self._len = 0
    self._index = []
    self._mask = 0
    return self
  def __iter__(self):
    return __set_iter__(self._items, self._len)
  def __pyc_tolist__(self):
    # list(s) / sorted(s) (tictactoe's `list(players)[0]`). Index loop
    # over the backing store, copied so callers can't alias/mutate the
    # set's internal _items.
    r = []
    i = 0
    while i < self._len:
      r.append(self._items[i])
      i += 1
    return r
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
  def difference(self, other):
    # tictactoe's `set(fields).difference(set([0]))`. Elements of self
    # not in other. Index loop over self, membership via other's while-
    # loop __contains__ (no iterator on either operand).
    r = set()
    i = 0
    while i < self._len:
      if not other.__contains__(self._items[i]):
        r.add(self._items[i])
      i += 1
    return r
  def __sub__(self, other):
    return self.difference(other)
  def intersection(self, other):
    r = set()
    i = 0
    while i < self._len:
      if other.__contains__(self._items[i]):
        r.add(self._items[i])
      i += 1
    return r
  def __and__(self, other):
    return self.intersection(other)
  def union(self, other):
    r = set()
    i = 0
    while i < self._len:
      r.add(self._items[i])
      i += 1
    for item in other:
      r.add(item)
    return r
  def __or__(self, other):
    return self.union(other)
  def symmetric_difference(self, other):
    r = set()
    i = 0
    while i < self._len:
      if not other.__contains__(self._items[i]):
        r.add(self._items[i])
      i += 1
    for item in other:
      if not self.__contains__(item):
        r.add(item)
    return r
  def __xor__(self, other):
    return self.symmetric_difference(other)
  # The in-place operators MUTATE self, as CPython's do, so an alias sees
  # the change. Without them `s |= t` fell back to issue 034's synthesized
  # `s = s | t`, which is CPython's fallback for a class with no
  # __ior__ -- right for a user class, wrong for set, which has one.
  # Each takes what it needs from `other` BEFORE mutating, so `s &= s`,
  # `s -= s` and `s ^= s` see an unmodified operand.
  def __ior__(self, other):
    return self.update(other)
  def intersection_update(self, other):
    keep = []
    i = 0
    while i < self._len:
      if other.__contains__(self._items[i]):
        keep.append(self._items[i])
      i += 1
    self._items = keep
    self._len = len(keep)
    self.__pyc_rehash__()
    return None
  def __iand__(self, other):
    self.intersection_update(other)
    return self
  def difference_update(self, other):
    items = []
    for item in other:
      items.append(item)
    for item in items:
      self.discard(item)
    return None
  def __isub__(self, other):
    self.difference_update(other)
    return self
  def symmetric_difference_update(self, other):
    items = []
    for item in other:
      items.append(item)
    for item in items:
      if self.__contains__(item):
        self.discard(item)
      else:
        self.add(item)
    return None
  def __ixor__(self, other):
    self.symmetric_difference_update(other)
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
def __pyc_set_from_iterable__(s, other):
  s.update(other)
  return s
