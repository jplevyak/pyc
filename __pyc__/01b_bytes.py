class bytes:
  # bytes shares str's exact length-prefixed char* buffer layout (see
  # sym_bytes registration, ifa/if1/ast.cc) -- every method below that
  # only touches the raw buffer (not a single element) reuses str's own
  # C helpers verbatim, just retyped. Indexing/iteration differ: CPython's
  # `bytes[i]` yields a plain int, not a length-1 bytes object (handled
  # by the sym_bytes branches added to ifa/analysis/fa.cc's
  # P_prim_index_object and the codegen in ifa/codegen/cg.cc /
  # cg_emit_llvm.cc, which route bytes indexing to _CG_int_from_string
  # instead of str's allocating _CG_char_from_string).
  def __add__(self, x):
    # NOT __pyc_operator__(self, "::", x) like str.__add__: the "::"
    # primitive (ifa/if1/prim_data.cc's prim_strcat) declares its operand
    #/result types as PRIM_TYPE_STRING specifically, which rejects
    # sym_bytes even though the underlying C call (_CG_strcat) doesn't
    # care -- __pyc_c_call__ has no such type-checked primitive in the
    # way, so it reuses the exact same C function directly.
    return __pyc_c_call__(bytes, "_CG_strcat", bytes, self, bytes, x)
  def __iadd__(self, x):
    return __pyc_c_call__(bytes, "_CG_strcat", bytes, self, bytes, x)
  def __mul__(self, l):
    return __pyc_c_call__(bytes, "_CG_string_mult", bytes, self, int, l)
  def __rmul__(self, l):
    # `n * self` (n an int): mirrors str.__rmul__/list.__rmul__ (issue
    # 025 R1) -- byte-string repetition is commutative too.
    return self.__mul__(l)
  def __str__(self):
    return self.__repr__()
  def __repr__(self):
    # ASCII passthrough only (v1): non-printable bytes are not
    # \x-escaped the way CPython's repr does. Known, documented gap --
    # add real escaping if a corpus program's output depends on it.
    return "b'" + self.decode() + "'"
  def __getitem__(self, key):
    return __pyc_primitive__(__pyc_symbol__("index_object"), self, key)
  def __pyc_getslice__(self, i, j, s):
    # Mirrors str.__pyc_getslice__ exactly -- same buffer layout, slicing
    # never touches a single element so the str/int split doesn't apply.
    return __pyc_c_call__(bytes, "_CG_string_getslice", bytes, self, int, i, int, j, int, s)
  def __len__(self):
    return __pyc_primitive__(__pyc_symbol__("len"), self)
  def __pyc_to_bool__(self):
    return self.__len__() != 0
  def __iter__(self):
    return __base_iter__(self)
  def __pyc_tolist__(self):
    # list(b"AB") -> [65, 66] -- CPython bytes semantics (contrast
    # str.__pyc_tolist__, which produces a list of 1-char strings).
    # Correct automatically once __getitem__/__iter__ yield int, no
    # extra coercion needed here (unlike bytearray's explicit casts).
    r = []
    for v in self:
      r.append(v)
    return r
  def __pyc_tobytes__(self):
    # bytes(some_bytes): identity, matching CPython.
    return self
  def __hash__(self):
    return __pyc_c_call__(int, "_CG_str_hash", bytes, self)
  def __eq__(self, x):
    return __pyc_c_call__(bool, "_CG_str_eq", bytes, self, bytes, x)
  def __ne__(self, x):
    return __pyc_c_call__(bool, "_CG_str_ne", bytes, self, bytes, x)
  def __lt__(self, x):
    return __pyc_c_call__(bool, "_CG_str_lt", bytes, self, bytes, x)
  def __le__(self, x):
    return __pyc_c_call__(bool, "_CG_str_le", bytes, self, bytes, x)
  def __gt__(self, x):
    return __pyc_c_call__(bool, "_CG_str_gt", bytes, self, bytes, x)
  def __ge__(self, x):
    return __pyc_c_call__(bool, "_CG_str_ge", bytes, self, bytes, x)
  def decode(self, encoding="utf-8"):
    # ASCII/latin-1-safe byte-for-byte reinterpretation of the same
    # underlying buffer as str -- not real codec-aware decoding (no
    # UTF-8 validation/multi-byte handling). `encoding` is accepted
    # for call-site compatibility and otherwise ignored.
    return __pyc_c_call__(str, "_CG_string_identity", bytes, self)
  def __mod__(self, t):
    # Narrow, CPython-compatible subset of bytes' %-format mini-language:
    # %c (one int arg, 0-255, -> that one byte) and literal %%. Covers
    # the corpus's actual usage (mandelbrot2's PPM pixel writer,
    # `b'%c%c%c%c' % (r,g,b,a)`) -- unlike str.__mod__, this deliberately
    # does NOT reuse __pyc_format_string__/_CG_format_string (that
    # primitive's FA transfer function returns sym_string
    # unconditionally, python_ifa_main.cc, so it can't type as bytes) and
    # does not implement %s/%d/%x/etc. Known gap; args must be a tuple
    # (no single-value non-tuple form).
    parts = []
    i = 0
    ti = 0
    n = len(self)
    while i < n:
      c = self[i]
      if c == ord('%') and i + 1 < n and self[i + 1] == ord('c'):
        parts.append(t[ti])
        ti += 1
        i += 2
      elif c == ord('%') and i + 1 < n and self[i + 1] == ord('%'):
        parts.append(c)
        i += 2
      else:
        parts.append(c)
        i += 1
    return bytes(parts)
