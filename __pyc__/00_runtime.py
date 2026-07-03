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
    return __pyc_operator__(__pyc_operator__(__pyc_symbol__("!"), self.__bool__().__pyc_to_bool__()),
                            __pyc_symbol__("&&"),
                            self.__len__() != 0)

class __pyc_None_type__:
  def __bool__(self):
    return False
  def __null__(self):
    return True
  def __str__(self):
    return "None"
  def __pyc_to_bool__(self):
    return False
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
  def __str__(self):
    if (self):
      return "True"
    else:
      return "False"
  def __pyc_to_bool__(self):
    return __pyc_clone_constants__(self)

class __base_iter__:
  thestr = None
  position = 0
  slen = 0
  def __init__(self, s):
    self.thestr = s
    self.slen = len(s)
  def __pyc_more__(self):
    return self.position < self.slen
  def __next__(self):
    self.position += 1
    return self.thestr.__getitem__(self.position-1)
