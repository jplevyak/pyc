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
    # CPython's bytes repr: printable ASCII as-is; \t \n \r and the
    # backslash escaped; everything else as \xNN (lowercase hex). The
    # quote is ' unless the value contains ' and no ", and whichever quote
    # is used is escaped inside. (Was an ASCII passthrough that printed
    # raw control bytes.)
    q = "'"
    has_sq = False
    has_dq = False
    for c in self:
      if c == 39:
        has_sq = True
      elif c == 34:
        has_dq = True
    if has_sq and not has_dq:
      q = '"'
    digits = "0123456789abcdef"
    r = "b" + q
    for c in self:
      if c == 92:
        r += "\\\\"
      elif c == 9:
        r += "\\t"
      elif c == 10:
        r += "\\n"
      elif c == 13:
        r += "\\r"
      elif chr(c) == q:
        r += "\\" + q
      elif c >= 32 and c < 127:
        r += chr(c)
      else:
        r += "\\x" + digits[c >> 4] + digits[c & 15]
    return r + q
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
  def join(self, seq):
    # minpng's `b''.join(img)`: bytes had no join at all (the member was
    # unresolved). Collect first -- `seq` may be a generator -- then join
    # in one allocation (_CG_string_join).
    parts = []
    for x in seq:
      parts.append(x)
    return __pyc_c_call__(bytes, "_CG_string_join", bytes, self, list, parts)
  # rstrip/upper/startswith: shedskin_examples/doom names its WAD lumps
  # with 8-byte `s` fields (`name.rstrip(b'\0').upper()`), which only had
  # a type once struct.unpack_from gave `s` fields one. bytes had none of
  # the three. The default `chars` is CPython's bytes whitespace set
  # (b' \t\n\r\x0b\x0c', which is what None means), so no None union.
  def rstrip(self, chars=b" \t\n\r\x0b\x0c"):
    j = len(self)
    m = len(chars)
    while j > 0:
      c = self[j - 1]
      k = 0
      while k < m and chars[k] != c:
        k += 1
      if k == m:
        break
      j -= 1
    return self.__pyc_getslice__(0, j, 1)
  def upper(self):
    # ASCII-only, like str.upper: same length-prefixed buffer layout.
    return __pyc_c_call__(bytes, "_CG_str_upper", bytes, self)
  def startswith(self, prefix):
    n = len(self)
    m = len(prefix)
    if m > n:
      return False
    i = 0
    while i < m:
      if self[i] != prefix[i]:
        return False
      i += 1
    return True
  def __contains__(self, x):
    # Subsequence test (`b'F_SKY' in name`, doom). CPython also accepts an
    # int byte value here; only the bytes form is implemented.
    n = len(self)
    m = len(x)
    i = 0
    while i + m <= n:
      j = 0
      while j < m and self[i + j] == x[j]:
        j += 1
      if j == m:
        return True
      i += 1
    return False
  def replace(self, old, new):
    # Same length-prefixed buffer as str, so str's one-pass helper serves.
    return __pyc_c_call__(bytes, "_CG_str_replace", bytes, self, bytes, old, bytes, new)
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
