__pyc_insert_c_header__('pyc_c_runtime.h')

class __pyc_any_type__:
  def __null__(self):
    return False
  def __pyc_to_bool__(self):
    # issues/089: __pyc_any_type__ is ifa's own universal top type
    # (sym_any, python_ifa_sym.cc renames it "__pyc_any_type__") --
    # every type in the lattice specializes it, including
    # closures/functions, which never connect to `object`'s
    # Python-specific class hierarchy at all (they're a core ifa
    # concept, not a user-defined class, so the "bare class inherits
    # object" rule in python_ifa_build_syms.cc never applies to them).
    # Without this, `if some_function:` / `bool(some_function)` had no
    # __pyc_to_bool__ candidate to dispatch to at all. object's own
    # __pyc_to_bool__ (bool()+len()-based) is strictly more specific
    # and still wins for anything that reaches it -- this is only the
    # fallback for receivers that don't, matching CPython's actual
    # default (any object, including any callable, is truthy unless
    # it overrides __bool__/__len__, which a bare function never does).
    return True
  def __not__(self):
    if self.__pyc_to_bool__():
      return False
    return True
  def __str__(self):
    return __pyc_primitive__(__pyc_symbol__("__pyc_to_str__"), self)
  def __pyc_tuplify__(self):
    return __pyc_primitive__(__pyc_symbol__("make_tuple"), self)
  def __pyc_seq_source__(self):
    # issues/110: the list make_seq copies from. Every iterable already
    # implements __pyc_tolist__ (str, bytes, tuple, range, set, dict),
    # so tuple(iterable) inherits list()'s whole iterable surface.
    # `class list` overrides this with identity.
    return self.__pyc_tolist__()
  def __pyc_getslice__(self, i, j, s):
    return self.__getitem__(slice(i,j,s))
  def __repr__(self):
    return self.__str__()
  def __hash__(self):
    # Identity hash, as CPython's object.__hash__ is, and as shedskin's
    # `long pyobj::__hash__() { return (intptr_t)this; }`. __hash__ existed
    # on str/bytes/numeric/list/tuple and NOWHERE ELSE, so `hash(x)` of a
    # class instance -- or of a function -- compiled with a warning and
    # then died with "matching function not found".
    #
    # On __pyc_any_type__ and NOT on `object`, which is where it was first
    # written and is wrong twice over. A closure never reaches `object`'s
    # class hierarchy at all (see __pyc_to_bool__ below for why), so
    # hash(some_function) still failed there; and defining it on BOTH
    # makes hash() a multi-candidate dispatch that aborts on a plain
    # instance. One definition, on the top type, covers everything.
    return __pyc_primitive__(__pyc_symbol__("id"), self)
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
  def __str__(self):
    return "<object>"
  def __pyc_tobytes__(self):
    # bytes(x) (python_ifa_build_if1.cc) dispatches to __pyc_tobytes__,
    # not CPython's real __bytes__ -- bytes/str/list define their own
    # __pyc_tobytes__ overrides below/elsewhere, but a plain user class
    # defining the real __bytes__ dunder (issues/025 TODO item 5,
    # tonyjpegdecoder's BMPFile) had no way to be reached. This default
    # bridges the two names; a class with neither gets the usual
    # "unresolved call '__bytes__'" compile-time reject, matching every
    # other unimplemented-dunder case in this codebase.
    return self.__bytes__()
  def __bool__(self):
    return True
  def __len__(self):
    return 1
  # issues/117: Python's REFLECTED ordering fallback. `a > b` first
  # tries `a.__gt__(b)`; when that does not exist it falls back to
  # `b.__lt__(a)`. pyc's frontend lowers `>` straight to a `__gt__`
  # call (python_ifa_build_if1.cc, PY_CMP_GT), so a class defining only
  # `__lt__` -- which is all Python asks of you, and what voronoi2's
  # Halfedge does -- dispatched to nothing and aborted at runtime with
  # "matching function not found".
  #
  # Defining the fallback HERE rather than in the dispatcher gets the
  # resolution order right for free: a class with its own __gt__
  # overrides this, and one with only __lt__ inherits the reflection.
  #
  # Deliberately only this direction. Reflecting __lt__ to __gt__ as
  # well would make a class defining NEITHER recurse for ever between
  # the two; leaving it undefined keeps that case reporting an
  # unresolved call exactly as it does today. CPython raises TypeError
  # there, so nothing correct is lost.
  def __gt__(self, x):
    return x.__lt__(self)
  def __ge__(self, x):
    return x.__le__(self)
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
  def __eq__(self, x):
    # CPython default: plain classes compare by identity unless they
    # override __eq__ (python_ifa_build_syms.cc's derive-compare comment:
    # "Python classes default to identity __eq__"). Without this, `a == b`
    # / `a != b` on two instances of a class with no explicit __eq__ is an
    # unresolved call (bh.py's `self != hg.pskip`, a plain Body vs Body
    # compare with no override).
    return __pyc_primitive__(__pyc_symbol__("is"), self, x)
  def __ne__(self, x):
    return not self.__eq__(x)

# issues/116: the bridge from CPython's iterator protocol to pyc's.
#
# pyc's for-loop is PEEK-THEN-FETCH -- `while it.__pyc_more__(): x =
# it.__next__()` -- while CPython's is fetch-until-StopIteration. Every
# iterator in __pyc__/ defines __pyc_more__, but it is not a Python
# method, so no user class and no ported library will ever define one:
# they write __iter__/__next__ and nothing else. `object` used to carry
# a `__pyc_more__` answering False, so such a class iterated ZERO
# times, silently and with no diagnostic. That default is gone with
# this bridge: the only shape it can still catch is a class whose
# `__iter__` returns something implementing NEITHER protocol, and for
# that an unresolved call is the right answer, not an empty loop.
#
# build_syms_pyda adds this class as a base of any class whose body
# defines `__next__` and not `__pyc_more__`, and installs that class's
# own `__next__` as `__pyc_user_next__`. The pair below then sits in
# front of it: __pyc_more__ fetches one value ahead and remembers
# whether there was one, __next__ hands back what was peeked. Exactly
# the shape __pyc_generator__ already uses for coroutine handles.
#
# Bridged classes pay one try/except per element; every builtin
# iterator keeps the cheap path, because they all define __pyc_more__
# and so are never given this base.
class __pyc_iterator__(object):
  __pyc_peek_primed__ = False
  __pyc_peek_has__ = False
  # No initializer for __pyc_peek__ on purpose: an `= 0` here would pin
  # the value channel to int for every bridged class, the exact bug
  # issues/114 fixed in __pyc_generator__. Its type comes from
  # __pyc_user_next__'s return type instead.
  def __pyc_more__(self):
    if not self.__pyc_peek_primed__:
      try:
        self.__pyc_peek__ = self.__pyc_user_next__()
        self.__pyc_peek_has__ = True
      except StopIteration:
        self.__pyc_peek_has__ = False
      self.__pyc_peek_primed__ = True
    return self.__pyc_peek_has__
  def __next__(self):
    # Also the entry point for a bare next(obj)/obj.__next__() outside
    # any loop: nothing has peeked, so this peeks and consumes in one
    # go, and re-raises StopIteration past exhaustion like CPython.
    if not self.__pyc_peek_primed__:
      self.__pyc_more__()
    self.__pyc_peek_primed__ = False
    if not self.__pyc_peek_has__:
      raise StopIteration(0)
    return self.__pyc_peek__
  def __contains__(self, item):
    # `x in it`. python_ifa_build_if1.cc lowers `in` to a direct
    # __contains__ dispatch with no fallback to the iterable protocol,
    # so without this a bridged class can't be membership-tested at all
    # -- the same gap __pyc_generator__.__contains__ fills (09_generator
    # .py). Consumes the iterator, which is what CPython's `in` does to
    # an iterator too.
    while self.__pyc_more__():
      if self.__next__() == item:
        return True
    return False
  def __pyc_tolist__(self):
    # list(it) / tuple(it) / anything reaching __pyc_seq_source__
    # (__pyc_any_type__ above). Same consuming semantics.
    r = []
    while self.__pyc_more__():
      r.append(self.__next__())
    return r
class __pyc_None_type__:
  def __bool__(self):
    return False
  def __eq__(self, x):
    # ifa/issues/090: `None == x` with None as the RECEIVER had no
    # method at all -- unresolved call '__eq__'. None equals nothing but
    # itself, which is identity, the same answer
    # __pyc_any_type__.__eq__ gives. Safe here unlike a __len__ or
    # __getitem__ stub (see the note further down): the result is a
    # bool, so nothing injects None into a container's element type.
    return __pyc_primitive__(__pyc_symbol__("is"), self, x)
  def __ne__(self, x):
    return not self.__eq__(x)
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
  # bool is an int subtype in Python, so ordering compares as 0/1
  # (`True > False`, `max([some_bools])`). Without these, any
  # ordering of bool values -- e.g. `max()`/`min()`/`sorted()` over a
  # list of comparison results, or a bare `a > b` on two bools --
  # dispatched to nothing ('unresolved call __gt__'); in a large
  # program that unresolved union also destabilized the splitter and
  # cascaded into unrelated NOTYPE collapses (shedskin chess's
  # `nonpawnAttacks` -> `max([... == ...])` poisoning `range(128)`).
  #
  # Expressed with the same branch-on-self form as __eq__ above, over
  # bool results only -- deliberately NOT via `int(self) < int(x)` or
  # a bare `<` primitive: the numeric comparison primitives reject a
  # bool operand ("illegal primitive argument type ... bool"), and the
  # int() route additionally miscompiles on the LLVM backend (int(True)
  # yields -1 there -- a separate bool sign-extension bug). These four
  # need no numeric primitive at all. (Mixed bool/int ordering like
  # `True < 5` is not the target here and is left to whatever the
  # numeric side supports.)
  def __lt__(self, x):     # self < x  : only False < True
    if self:
      return False
    else:
      return x
  def __le__(self, x):     # self <= x : false only for True <= False
    if self:
      return x
    else:
      return True
  def __gt__(self, x):     # self > x  : only True > False
    if self:
      return not x
    else:
      return False
  def __ge__(self, x):     # self >= x : false only for False >= True
    if self:
      return True
    else:
      return not x
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
