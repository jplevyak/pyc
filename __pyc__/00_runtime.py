__pyc_insert_c_header__('pyc_c_runtime.h')

class __pyc_any_type__:
  def __null__(self):
    return False
  def __str__(self):
    return __pyc_primitive__(__pyc_symbol__("__pyc_to_str__"), self)
  def __pyc_tuplify__(self):
    return __pyc_primitive__(__pyc_symbol__("make_tuple"), self)
  def __pyc_getslice__(self, i, j, s):
    return self.__getitem__(slice(i,j,s))
  def __repr__(self):
    return self.__str__()
  def __deepcopy__(self):
    # issues/029 fallback: value types (scalars, strings) and shapes
    # with no per-field recursion (tuples, closures) deep-copy as a
    # shallow copy -- the copy prim is identity for scalars and a
    # one-level struct clone otherwise. Record classes get a
    # SYNTHESIZED recursive override (gen_class_pyda), lists a
    # handwritten one (04_sequence.py).
    return __pyc_primitive__(__pyc_symbol__("copy"), self)
  def __format__(self, spec):
    # issues/006: default __format__ for classes with no override.
    # CPython's object.__format__ raises TypeError for a non-empty
    # spec; pyc has no exception model yet (issue 011), so this
    # falls back to str() for any spec rather than failing at
    # runtime -- permissive, not exactly CPython's behavior.
    return self.__str__()
  # Issue 028 step 4: `is` / `is not` no longer dispatch to
  # these methods.  The frontend (`python_ifa_build_if1.cc`
  # PY_compare) now lowers ALL `is`/`is not` to one of:
  #   - x is None       → prim_isinstance(x, sym_nil_type)
  #   - None is x       → same
  #   - None is None    → True (constant)
  #   - x is y (other)  → prim_is(x, y)         ← real
  #                                                identity
  # `prim_is` lowers to pointer equality at C and v2 LLVM
  # codegen, matching CPython's identity semantics for
  # non-None operands.
  #
  # These method stubs remain in case someone writes
  # `x.__is__(y)` explicitly; they keep the historical
  # always-False / always-True behavior for backward
  # compatibility, but no idiomatic Python should reach
  # them.
  def __is__(self, x):
    return False
  def __nis__(self, x):
    return True

class object:
  def __null__(self):
    return False
  def __pyc_more__(self):
    return False
  def __str__(self):
    return "<object>"
  def __bool__(self):
    return True
  def __len__(self):
    return 1
  def __pyc_to_bool__(self):
    # Default truthiness: __bool__() (True unless overridden) combined
    # with __len__() != 0 (1 unless overridden), so a class overriding
    # either gets Python-ish behavior. The previous form negated the
    # __bool__ operand (`(!__bool__()) && ...`), inverting truthiness
    # for every object-derived class (issue 025).
    if self.__bool__():
      return self.__len__() != 0
    return False
  def __not__(self):
    # `not x` sends __not__ straight at x (PY_bool_not); without this
    # fallback a user object -- or a None|T optional-field union, the
    # common `if not self.field:` idiom -- has no receiver and FA
    # reports "expression has no type" (issue 025).
    if self.__pyc_to_bool__():
      return False
    return True

class __pyc_None_type__:
  def __bool__(self):
    return False
  def __null__(self):
    return True
  def __str__(self):
    return "None"
  def __pyc_to_bool__(self):
    return False
  def __not__(self):
    return True
  def __deepcopy__(self):
    # None is immutable; deepcopy is identity (also keeps the nil
    # member of Optional[T] fields typed through the synthesized
    # per-class __deepcopy__ recursion, issues/029).
    return self
  def __pyc_getslice__(self, i, j, s):
    # Container ops on an Optional[container] field's nil member:
    # `field[:k]` / `field[k]` where the field starts as None and is
    # truth-guarded before use (`if node.args:` -- genetic2's
    # crossover). The nil arm is runtime-dead behind the guard, but
    # FA doesn't narrow attribute loads (ifa/issues/046 family), so
    # without these the nil part of the union routes into the
    # __pyc_any_type__ fallback and the whole expression loses its
    # type. Returns [] (not self/None): the slice result flows into
    # concatenation on the dead arm, and a None operand re-poisons
    # list.__add__ (sizeof_element of non-container). CPython would
    # raise TypeError -- pyc has no exception model (issues/011), so
    # this is the documented degradation.
    return []
  # NB deliberately NO __getitem__ or __len__ stubs: every container
  # class-body field defaulting to None (__tuple_iter__.thetuple,
  # __list_iter__.thelist, ...) makes {nil, T} unions at THEIR uses,
  # and a stub turns those sites into live multi-candidate dispatches
  # program-wide -- a __getitem__ stub injected None into element
  # unions (printing tuple element 0 became "None": the {nil,int64}
  # str-dispatch null-tests the SCALAR, and 0 is indistinguishable
  # from NULL), and even the value-safe __len__ stub regressed the
  # whole LLVM suite (its dispatch emitter lacks the C backend's
  # nil/untagged routes). The nil arm of an unresolved send is
  # silently dropped under fruntime_errors, which is the right
  # degradation for these.
  # Issue 004: None.__is__(x) is True iff x is also None.
  # Note: the __pyc_None_type__::__is__ path is rarely hit
  # in practice because the frontend (PY_CMP_IS) rewrites
  # `x is None` and `None is x` directly to prim_isinstance
  # against sym_nil_type — bypassing the method dispatch
  # entirely (issue 024).  This stays as a fallback for the
  # case where neither operand is statically the None
  # constant.
  def __is__(self, x):
    return x.__null__()
  def __nis__(self, x):
    if x.__null__():
      return False
    return True

class bool:
  def __and__(self, x):
    if (self):
      return x
    else:
      return self
  def __or__(self, x):
    if (self):
      return self
    else:
      return x
  def __not__(self):
    if (self):
      return False
    else:
      return True
  def __eq__(self, x):
    if (self):
      return x
    else:
      return not x
  def __ne__(self, x):
    return not self.__eq__(x)
  def __str__(self):
    if (self):
      return "True"
    else:
      return "False"
  def __pyc_to_bool__(self):
    return __pyc_clone_constants__(self)
  def __format__(self, spec):
    # issues/006: PEP 3101 format-spec mini-language. Matches CPython:
    # bool is an int subtype, so a numeric spec ("d", "x", width, etc.)
    # formats 0/1 as an int; an empty spec falls back to str().
    if len(spec) == 0:
      return self.__str__()
    v = 0
    if self:
      v = 1
    return __pyc_c_call__(str, "_CG_format_int_spec", int, v, str, spec)

class __base_iter__:
  thestr = None
  position = 0
  slen = 0
  def __iter__(self):
    # Iterators are self-iterable (Python protocol) -- lets
    # `for x in it:` consume an already-made iterator (functools
    # .reduce, issue 025).
    return self
  def __init__(self, s):
    self.thestr = s
    self.slen = len(s)
  def __pyc_more__(self):
    return self.position < self.slen
  def __next__(self):
    self.position += 1
    return self.thestr.__getitem__(self.position-1)
