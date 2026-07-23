class __list_iter__:
  thelist = None
  position = 0
  def __iter__(self):
    # Iterators are self-iterable (Python protocol) -- lets
    # `for x in it:` consume an already-made iterator (functools
    # .reduce, issue 025).
    return self
  def __init__(self, l):
    # __pyc_clone_constants__ on the ctor param puts __list_iter__ on
    # the ifa/issues/045 clone_methods_per_cs track (same lever as
    # range, see __pyc__/05_builtins.py): each creating contour gets
    # its OWN iterator CS (creation_point skips split-parent CS
    # reuse) and __pyc_more__/__next__ split per receiver CS in the
    # PER_CS_RECEIVER stage. Without it, every `for x in lst:` in the
    # program shares iterator CSs whose `thelist` unions ALL iterated
    # lists, so `x` is the union of every loop's element types --
    # which re-fused the per-recursion-level contours the
    # recursive-ES splitting fix separates (ifa/issues/043 shape C;
    # deepcopy over nested lists was the repro).
    self.thelist = __pyc_clone_constants__(l)
  def __pyc_more__(self):
    return self.position < len(self.thelist)
  def __next__(self):
    self.position += 1
    return self.thelist.__getitem__(self.position-1)

class list:
  def __len__(self):
    return __pyc_primitive__(__pyc_symbol__("len"), self)
  def __getitem__(self, key):
    return __pyc_primitive__(__pyc_symbol__("index_object"), self, __pyc_clone_constants__(key))
  def __pyc_getslice__(self, i, j, s):
    return __pyc_c_call__(__pyc_primitive__(__pyc_symbol__("merge"), self, self),
                          "_CG_list_getslice",
                          list, self,
                          int, __pyc_primitive__(__pyc_symbol__("sizeof_element"), self),
                          int, i,
                          int, j,
                          int, s)
  def __pyc_setslice__(self, i, j, s, v):
    return __pyc_c_call__(__pyc_primitive__(__pyc_symbol__("merge_in"), self, v),
                          "_CG_list_setslice",
                          list, self,
                          int, __pyc_primitive__(__pyc_symbol__("sizeof_element"), self),
                          int, i,
                          int, j,
                          list, v)
  def __setitem__(self, key, value):
    return __pyc_primitive__(__pyc_symbol__("set_index_object"), self,
                             __pyc_clone_constants__(key), value)
  def __delitem__(self, key):
    pass
  def __iter__(self):
    return __list_iter__(self)
  def __contains__(self, item):
    for x in self:
      if x == item:
        return True
    return False
  def __pyc_tolist__(self):
    # list(xs) copies -- see the list() intercept (issue 025).
    r = []
    for x in self:
      r.append(x)
    return r
  def __add__(self, l):
    # list + tuple: build the result with append loops (isinstance
    # branch stays dead in list+list contours, same narrowing
    # pattern as int.__mul__'s reflected dispatch). A tuple is a
    # fixed-arity struct, not list layout, so _CG_list_add can't
    # take it directly. Built INLINE rather than recursing through
    # self.__add__(l.__pyc_tolist__()): the recursive form routed
    # every list+tuple site through shared conversion contours whose
    # merged types cross-contaminated unrelated sites' element types
    # (two different-class list+tuple concats in one program produced
    # union/bottom element types). Arises naturally now that dynamic
    # tuple() returns a list (genetic2's crossover:
    # `args[:k] + (node,) + args[k+1:]`).
    if isinstance(l, tuple):
      r = []
      for k in range(len(self)):
        r.append(self[k])
      for k in range(len(l)):
        r.append(l[k])
      return r
    return __pyc_c_call__(__pyc_primitive__(__pyc_symbol__("merge_in"), self, l),
                          "_CG_list_add",
                          list, self,
                          int, l,
                          int, __pyc_primitive__(__pyc_symbol__("sizeof_element"), self),
                          int, __pyc_primitive__(__pyc_symbol__("sizeof_element"), l))
  def __radd__(self, l):
    pass
  def __iadd__(self, l):
    # NB self.__add__(l), not bare __add__(self, l): class bodies are
    # not enclosing scopes for their methods (find_PycSymbol skips
    # non-current class scopes per Python semantics -- a bare name in
    # a method body resolves to the module, not to a sibling method).
    return self.__add__(l)
  def __mul__(self, n):
    return __pyc_c_call__(__pyc_primitive__(__pyc_symbol__("merge"), self, self),
                          "_CG_list_mult",
                          list, self,
                          int, n,
                          int, __pyc_primitive__(__pyc_symbol__("sizeof_element"), self), ")")
  def __rmul__(self, n):
    # `n * self` (n an int): list repetition is commutative, so reuse
    # __mul__ (issue 025 R1 "missing sequence ops").
    return self.__mul__(n)
  def __imul__(self, l):
    pass
#  @must_specialize("l:list")
  def __eq__(self, l):
    ll = __pyc_clone_constants__(len(l))
    lself = __pyc_clone_constants__(len(self))
    if lself != ll:
      return False
    for i in range(lself):
      if l[i] != self[i]:
        return False
    return True
  def __ne__(self, l):
    return not self.__eq__(l)
  def append(self, x):
    l = __pyc_primitive__(__pyc_symbol__("len"), self)
    tmp = __pyc_c_call__(__pyc_primitive__(__pyc_symbol__("merge_in"), self, self),
                         "_CG_list_resize",
                         list, self,
                         int, __pyc_primitive__(__pyc_symbol__("sizeof_element"), self),
                         int, l + 1)
    tmp.__setitem__(l, x)
    return tmp
  def extend(self, other):
    # issue 025 R1 "missing sequence ops": extend was an unknown
    # method on list -- silently warned and dropped the call rather
    # than erroring, so `a.extend([2,3])` was a silent no-op
    # (softrender/chull/rdb). append()'s merge_in-tagged resize
    # mutates self's backing store in place (verified: a bare
    # `l.append(x)` inside a helper function already mutates the
    # caller's list with no explicit `l = l.append(x)` rebind
    # needed), so a plain per-element append loop is enough.
    for x in other:
      self.append(x)
    return None
  def index(self, x):
    # Returns -1 when absent instead of raising ValueError (no
    # exception model, issue 011).
    n = len(self)
    i = 0
    while i < n:
      if self[i] == x:
        return i
      i += 1
    return -1
  def count(self, x):
    n = len(self)
    c = 0
    i = 0
    while i < n:
      if self[i] == x:
        c += 1
      i += 1
    return c
  def reverse(self):
    i = 0
    j = len(self) - 1
    while i < j:
      t = self[i]
      self[i] = self[j]
      self[j] = t
      i += 1
      j -= 1
    return None
  def sort(self, key=None, reverse=False):
    # Stable insertion sort (matches Python's stability guarantee;
    # reverse=True flips the comparison rather than reversing after,
    # which is what Python's stability under reverse means). The
    # key-is-None branches keep each call contour monomorphic via
    # nil narrowing (same pattern as min/max in 05_builtins.py) --
    # `key(x)`'s type never unions with the element type. Issue 025:
    # circle.py's `circles.sort(key=lambda c: c.offset())` was its
    # first blocker.
    # Comparisons use ONLY `<` (CPython's sort contract: elements
    # need just __lt__ -- voronoi2's Site defines __lt__/__eq__ and
    # nothing else, and pyc has no reflected-operator fallback).
    n = len(self)
    i = 1
    while i < n:
      x = self[i]
      j = i - 1
      if key is None:
        if reverse:
          while j >= 0 and self[j] < x:
            self[j + 1] = self[j]
            j = j - 1
        else:
          while j >= 0 and x < self[j]:
            self[j + 1] = self[j]
            j = j - 1
      else:
        kx = key(x)
        if reverse:
          while j >= 0 and key(self[j]) < kx:
            self[j + 1] = self[j]
            j = j - 1
        else:
          while j >= 0 and kx < key(self[j]):
            self[j + 1] = self[j]
            j = j - 1
      self[j + 1] = x
      i = i + 1
    return None
  def __str__(self):
    x = "["
    for k in range(0,len(self)):
      if (k):
        x += ", "
      x += self[k].__repr__()
    x += "]"
    return x
  def __deepcopy__(self):
    # issues/029: element-recursive list copy. Elements dispatch
    # their own __deepcopy__ (records: synthesized per-class;
    # scalars/strings: the any-type shallow fallback; None:
    # identity). Index loop per house style.
    r = []
    for k in range(len(self)):
      r.append(self[k].__deepcopy__())
    return r
  def __pyc_to_bool__(self):
    return self.__len__() != 0

class __tuple_iter__:
  thetuple = None
  position = 0
  def __iter__(self):
    # Iterators are self-iterable (Python protocol) -- lets
    # `for x in it:` consume an already-made iterator (functools
    # .reduce, issue 025).
    return self
  def __init__(self, t):
    self.thetuple = t
  def __pyc_more__(self):
    return self.position < len(self.thetuple)
  def __next__(self):
    self.position += 1
    return self.thetuple.__getitem__(self.position-1)

class tuple:
  def __getitem__(self, key):
    return __pyc_primitive__(__pyc_symbol__("index_object"), self, __pyc_clone_constants__(key))
  def __setitem__(self, key, value):
    return __pyc_primitive__(__pyc_symbol__("set_index_object"), self, __pyc_clone_constants__(key), value)
  def __iter__(self):
    return __tuple_iter__(self)
  def __len__(self):
    return __pyc_clone_constants__(__pyc_primitive__(__pyc_symbol__("len"), self))
  def __contains__(self, item):
    # `x in (a, b, c)` (collatz's `rest9 in (2, 4, 5, 8)`). INDEX loop,
    # not `for x in self`: sharing one __tuple_iter__ CS across
    # different-arity tuples cross-wires per-arity len/method slots
    # (ifa/issues/047) -- the same reason __pyc_tolist__/__str__ index.
    for k in range(len(self)):
      if self[k] == item:
        return True
    return False
  def __pyc_to_bool__(self):
    return self.__len__() != 0
  def __pyc_tolist__(self):
    # list(t) and dynamic tuple(xs) both land here via the
    # list()/tuple() intercepts (python_ifa_build_if1.cc, issue
    # 025). Dynamic-length tuple() returns a LIST: pyc tuples are
    # fixed-arity structs, so tuple(iterable) can't be a true tuple
    # -- same documented compromise as zip/map/filter/enumerate/
    # reversed returning lists (indexing/iteration/len behave
    # identically; printing/hashing differ).
    #
    # INDEX loop, not `for x in self`: a program that iterates two
    # DIFFERENT-arity tuples shares one __tuple_iter__ CS whose
    # folded per-arity len and prototype method-pointer slots cross
    # wires (segfault -- pre-existing at user level too, see
    # ifa/issues/047). Indexing (like tuple.__str__'s loop) never
    # touches the iterator.
    r = []
    for k in range(len(self)):
      r.append(self[k])
    return r
  def __pyc_tuplify__(self):
    return self
  def __add__(self, t):
    # Dynamic tuple concatenation returns a LIST -- fixed-arity
    # structs can't concatenate at runtime; the compile-time
    # literal fold (try_fold_tuple_arity, issues/025 R1 item 4)
    # handles `(a, b) + (c,)` shapes, this covers NAMED tuple
    # values (chess's `queenLines = bishopLines + rookLines`).
    # Same list-for-dynamic-tuple compromise as tuple()/zip/map.
    # Inline index loops, not shared conversion helpers (see
    # list.__add__'s cross-contamination note).
    r = []
    for k in range(len(self)):
      r.append(self[k])
    for k in range(len(t)):
      r.append(t[k])
    return r
  # __eq__/__lt__ are primitives (issue 025, tictactoe): a Python
  # element loop indexes self[i]/t[i] with a VARIABLE i, which collapses
  # a heterogeneous fixed-arity tuple to the union of ALL its element
  # types -- so `a < b` compares e.g. int against tuple and fails FA.
  # The primitive's codegen (cg.cc/cg_emit_llvm.cc) instead compares
  # field-by-field using each operand's CONCRETE field types, recursing
  # for nested tuples and handling differing arity per Python semantics
  # (== across arity is False; < uses the common-prefix / shorter-is-less
  # rule).
  def __eq__(self, t):
    return __pyc_primitive__(__pyc_symbol__("tuple_eq"), self, t)
  def __ne__(self, t):
    return not self.__eq__(t)
  def __lt__(self, t):
    return __pyc_primitive__(__pyc_symbol__("tuple_lt"), self, t)
  def __le__(self, t):
    return not t.__lt__(self)
  def __gt__(self, t):
    return t.__lt__(self)
  def __ge__(self, t):
    return not self.__lt__(t)
  def __str__(self):
    n = len(self)
    x = "("
    for k in range(0, n):
      if (k):
        x += ", "
      x += self[k].__repr__()
    if n == 1:
      x += ","
    x += ")"
    return x
