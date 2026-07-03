class int:
#  @must_specialize("x:anynum") -- for dispatching to __radd__
#  def __add__(self, x):
#    return __pyc_operator__(self, "+", x)
#  def __add__(self, x):
#    return x.__radd__(self)
  def __add__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("+"), __pyc_clone_constants__(x))
  def __sub__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("-"), __pyc_clone_constants__(x))
  def __mul__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("*"), __pyc_clone_constants__(x))
  def __truediv__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("/"), __pyc_clone_constants__(x))
  def __mod__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("%"), __pyc_clone_constants__(x))
  def __pow__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("**"), __pyc_clone_constants__(x))
  def __lshift__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("<<"), __pyc_clone_constants__(x))
  def __rshift__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__(">>"), __pyc_clone_constants__(x))
  def __or__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("|"), __pyc_clone_constants__(x))
  def __xor__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("^"), __pyc_clone_constants__(x))
  def __and__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("&"), __pyc_clone_constants__(x))
  def __floordiv__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("/"), __pyc_clone_constants__(x))
  def __iadd__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("+"), __pyc_clone_constants__(x))
  def __isub__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("-"), __pyc_clone_constants__(x))
  def __imul__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("*"), __pyc_clone_constants__(x))
  def __itruediv__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("/"), __pyc_clone_constants__(x))
  def __imod__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("%"), __pyc_clone_constants__(x))
  def __ipow__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("**"), __pyc_clone_constants__(x))
  def __ilshift__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("<<"), __pyc_clone_constants__(x))
  def __irshift__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__(">>"), __pyc_clone_constants__(x))
  def __ior__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("|"), __pyc_clone_constants__(x))
  def __ixor__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("^"), __pyc_clone_constants__(x))
  def __iand__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("&"), __pyc_clone_constants__(x))
  def __ifloordiv__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("/"), __pyc_clone_constants__(x))
  def __invert__(self):
    return __pyc_operator__(__pyc_symbol__("~"), __pyc_clone_constants__(self))
  def __pos__(self):
    return __pyc_operator__(__pyc_symbol__("+"), __pyc_clone_constants__(self))
  def __neg__(self):
    return __pyc_operator__(__pyc_symbol__("-"), __pyc_clone_constants__(self))
  def __not__(self):
    return __pyc_operator__(__pyc_symbol__("!"), __pyc_clone_constants__(self))
  def __eq__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("=="), __pyc_clone_constants__(x))
  def __ne__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("!="), __pyc_clone_constants__(x))
  def __lt__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("<"), __pyc_clone_constants__(x))
  def __le__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__("<="), __pyc_clone_constants__(x))
  def __gt__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__(">"), __pyc_clone_constants__(x))
  def __ge__(self, x):
    return __pyc_operator__(__pyc_clone_constants__(self), __pyc_symbol__(">="), __pyc_clone_constants__(x))
  def __hash__(self):
    return self
  def __bool__(self):
     return __pyc_clone_constants__(self) != 0
  def __null__(self):
     return False
  def __str__(self):
    # D.4: library implementation of int → str via libc snprintf,
    # replacing the C++ emit_prim_to_string path. _CG_str_from_int
    # is a typed runtime helper that allocates a pyc str buffer
    # and formats self into it. Both backends resolve the symbol
    # against the same definition: static-inline copy in
    # pyc_c_runtime.h for the C backend, libpyc_runtime.a for the
    # LLVM backend (Phase D.3.5).
    return __pyc_c_call__(str, "_CG_str_from_int",
                          int, __pyc_clone_constants__(self))
  def __pyc_to_bool__(self):
    return __pyc_clone_constants__(self) != 0
  def __format__(self, spec):
    # issues/006: PEP 3101 format-spec mini-language (f-string
    # `{x:spec}` / `format(x, spec)`). _CG_format_int_spec parses
    # `spec` and does the actual formatting/padding in C.
    return __pyc_c_call__(str, "_CG_format_int_spec",
                          int, __pyc_clone_constants__(self), str, spec)

class float:
  def __add__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("+"), x)
  def __sub__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("-"), x)
  def __mul__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("*"), x)
  def __truediv__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("/"), x)
  def __mod__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("%"), x)
  def __pow__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("**"), x)
  def __floordiv__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("/"), x)
  def __iadd__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("+"), x)
  def __isub__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("-"), x)
  def __imul__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("*"), x)
  def __itruediv__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("/"), x)
  def __imod__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("%"), x)
  def __ipow__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("**"), x)
  def __ifloordiv__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("/"), x)
  def __invert__(self):
    return __pyc_operator__(__pyc_symbol__("~"), self)
  def __pos__(self):
    return __pyc_operator__(__pyc_symbol__("+"), self)
  def __neg__(self):
    return __pyc_operator__(__pyc_symbol__("-"), self)
  def __not__(self):
    return __pyc_operator__(__pyc_symbol__("!"), self)
  def __eq__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("=="), x)
  def __ne__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("!="), x)
  def __lt__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("<"), x)
  def __le__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("<="), x)
  def __gt__(self, x):
    return __pyc_operator__(self, __pyc_symbol__(">"), x)
  def __ge__(self, x):
    return __pyc_operator__(self, __pyc_symbol__(">="), x)
  def __bool__(self):
    return self != 0.0
  def __null__(self):
    return False
  def __str__(self):
    # D.5: library implementation of float → str. _CG_str_from_float
    # uses %.17g for round-trip precision, then appends ".0" for
    # whole-number values so CPython's `str(0.0)` == "0.0" parity
    # is preserved. Replaces emit_prim_to_string's float branch.
    return __pyc_c_call__(str, "_CG_str_from_float", float, self)
  def __pyc_to_bool__(self):
    return self != 0.0
  def __format__(self, spec):
    # issues/006: PEP 3101 format-spec mini-language, see int.__format__.
    return __pyc_c_call__(str, "_CG_format_float_spec", float, self, str, spec)
