class str:
  def __add__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("::"), x)
  def __iadd__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("::"), x)
  def __str__(self):
    return self
  def __repr__(self):
    # CPython's quoting and escaping (_CG_str_repr); was "'" + self + "'".
    return __pyc_c_call__(str, "_CG_str_repr", str, self)
  def __getitem__(self, key):
    return __pyc_primitive__(__pyc_symbol__("index_object"), self, key)
  def __pyc_getslice__(self, i, j, s):
    # issue 025: str had no __pyc_getslice__ of its own (unlike
    # list/range/bytearray) -- `text[i:j]` fell through to
    # __pyc_any_type__'s generic self.__getitem__(slice(i,j,s))
    # fallback, but __getitem__ above unconditionally treats its key
    # as a single int index (index_object), so it received a slice
    # *object* where an int was expected. Miscompiled to invalid C
    # (_CG_char_from_string given a struct pointer instead of an
    # int) with a clean compile otherwise -- even the simplest
    # `"hello"[1:3]` hit this; string slicing had no test coverage
    # before this fix. Mirrors list.__pyc_getslice__'s shape.
    return __pyc_c_call__(str, "_CG_string_getslice", str, self, int, i, int, j, int, s)
  def __len__(self):
    return __pyc_primitive__(__pyc_symbol__("len"), self)
  def __pyc_to_bool__(self):
    return self.__len__() != 0
  def __iter__(self):
    return __base_iter__(self)
  def __pyc_tolist__(self):
    # list("abc") -> ["a", "b", "c"] -- see the list() intercept in
    # python_ifa_build_if1.cc (issue 025; block.py's first blocker).
    r = []
    for c in self:
      r.append(c)
    return r
  # int(s, base) (python_ifa_build_if1.cc lowers it to this).
  def __pyc_int_base__(self, base):
    return __pyc_c_call__(int, "_CG_str_to_int64_base", str, self, int, base)
  def __pyc_ord__(self):
    return __pyc_c_call__(int, "_CG_ord", str, self)
  def __pyc_tobytes__(self):
    # bytes(some_str): same reinterpretation as encode() below (the
    # bytes(x) builtin call intercepts to x.__pyc_tobytes__(), mirroring
    # str(x)/list(x)'s existing dispatch).
    return self.encode()
  def encode(self, encoding="utf-8"):
    # ASCII/latin-1-safe byte-for-byte reinterpretation of the same
    # underlying buffer as bytes -- not real codec-aware encoding.
    # `encoding` is accepted for call-site compatibility and otherwise
    # ignored. Mirrors bytes.decode()'s identity cast the other way.
    return __pyc_c_call__(bytes, "_CG_string_identity", str, self)
  def __mul__(self, l):
    return __pyc_c_call__(str, "_CG_string_mult", str, self, int, l)
  def __rmul__(self, l):
    # `n * self` (n an int): string repetition is commutative, so
    # reuse __mul__ (mirrors list.__rmul__, issue 025 R1).
    return self.__mul__(l)
  def __hash__(self):
    return __pyc_c_call__(int, "_CG_str_hash", str, self)
  def __eq__(self, x):
    # CPython: a str never equals a non-str (`"a" == 1` is False). Passing
    # the other operand to _CG_str_eq as a str compiled to a run-time
    # "C call argument type mismatch" abort. isinstance folds per contour.
    if isinstance(x, str):
      return __pyc_c_call__(bool, "_CG_str_eq", str, self, str, x)
    return False
  def __ne__(self, x):
    if isinstance(x, str):
      return __pyc_c_call__(bool, "_CG_str_ne", str, self, str, x)
    return True
  def __lt__(self, x):
    return __pyc_c_call__(bool, "_CG_str_lt", str, self, str, x)
  def __le__(self, x):
    return __pyc_c_call__(bool, "_CG_str_le", str, self, str, x)
  def __gt__(self, x):
    return __pyc_c_call__(bool, "_CG_str_gt", str, self, str, x)
  def __ge__(self, x):
    return __pyc_c_call__(bool, "_CG_str_ge", str, self, str, x)
  def __mod__(self, t):
    return __pyc_primitive__(__pyc_symbol__("__pyc_format_string__"), self, t)
  # CPython's fallback for a type with no in-place method: `x op= y` is
  # `x = x op y`. Issue 034 synthesizes it for record classes only, so a
  # builtin value type spells it out (rdb's `basis &= MatchRule(...)`).
  def __imul__(self, x):
    return self.__mul__(x)
  def __imod__(self, x):
    return self.__mod__(x)
  def __format__(self, spec):
    # issues/006: PEP 3101 format-spec mini-language, see int.__format__.
    return __pyc_c_call__(str, "_CG_format_str_spec", str, self, str, spec)
  def join(self, seq):
    # issues/050: collect once (`seq` may be a generator), then join in one
    # allocation, as bytes.join does. Pairwise `r = r + x` was O(n^2):
    # 400 000 one-char parts took over a minute.
    parts = []
    for x in seq:
      parts.append(x)
    return __pyc_c_call__(str, "_CG_string_join", str, self, list, parts)
  def lower(self):
    # ASCII case maps, one allocation each (issues/050).
    return __pyc_c_call__(str, "_CG_str_lower", str, self)
  def upper(self):
    return __pyc_c_call__(str, "_CG_str_upper", str, self)
  def isupper(self):
    # ASCII-only, matching upper()/lower() above. CPython: True iff
    # every cased character is uppercase AND at least one cased
    # character exists (digits/punctuation don't count either way).
    n = len(self)
    has_cased = False
    i = 0
    while i < n:
      o = ord(self[i])
      if o >= 97 and o <= 122:
        return False
      if o >= 65 and o <= 90:
        has_cased = True
      i += 1
    return has_cased
  def islower(self):
    # issues/118: the mirror of isupper above, and missing until now --
    # calling it produced "getter not resolved" at runtime, with only an
    # opaque "illegal call argument type expression" at compile time to
    # go on. ASCII-only, like every other case method here.
    n = len(self)
    has_cased = False
    i = 0
    while i < n:
      o = ord(self[i])
      if o >= 65 and o <= 90:
        return False
      if o >= 97 and o <= 122:
        has_cased = True
      i += 1
    return has_cased
  def isspace(self):
    # issues/118. CPython: True iff the string is non-empty and every
    # character is whitespace. The set here is ASCII whitespace: space,
    # \t, \n, \v, \f, \r.
    n = len(self)
    if n == 0:
      return False
    i = 0
    while i < n:
      o = ord(self[i])
      if o != 32 and (o < 9 or o > 13):
        return False
      i += 1
    return True
  def swapcase(self):
    # issues/118. ASCII-only, consistent with upper()/lower().
    return __pyc_c_call__(str, "_CG_str_swapcase", str, self)
  def __contains__(self, x):
    return __pyc_c_call__(int, "_CG_str_find", str, self, str, x, int, 0, int, len(self)) >= 0
  def __pyc_substr__(self, i, j):
    # self[i:j] for non-negative i, j, in one allocation (issues/050; it
    # was a char-by-char concat).
    return __pyc_c_call__(str, "_CG_str_substr", str, self, int, i, int, j)
  # strip/lstrip/rstrip share one scanner. The default `chars` is the
  # ASCII part of str.isspace() (what CPython's None means), so the
  # parameter is always a str and there is no None union. go reads
  # `readline().rstrip('\n')` (issues/025).
  def __pyc_strip_from__(self, chars, i, n):
    m = len(chars)
    while i < n:
      c = self[i]
      k = 0
      while k < m and chars[k] != c:
        k += 1
      if k == m:
        break
      i += 1
    return i
  def __pyc_strip_to__(self, chars, i, j):
    m = len(chars)
    while j > i:
      c = self[j - 1]
      k = 0
      while k < m and chars[k] != c:
        k += 1
      if k == m:
        break
      j -= 1
    return j
  def strip(self, chars=" \t\n\r\x0b\x0c\x1c\x1d\x1e\x1f"):
    n = len(self)
    i = self.__pyc_strip_from__(chars, 0, n)
    return self.__pyc_substr__(i, self.__pyc_strip_to__(chars, i, n))
  def lstrip(self, chars=" \t\n\r\x0b\x0c\x1c\x1d\x1e\x1f"):
    n = len(self)
    return self.__pyc_substr__(self.__pyc_strip_from__(chars, 0, n), n)
  def rstrip(self, chars=" \t\n\r\x0b\x0c\x1c\x1d\x1e\x1f"):
    return self.__pyc_substr__(0, self.__pyc_strip_to__(chars, 0, len(self)))
  def split(self, sep=None, maxsplit=-1):
    # sep=None: runs of whitespace, no empty tokens (Python
    # semantics). String sep: split on every occurrence, empty
    # tokens included. maxsplit >= 0 caps the number of splits; the
    # rest of the string is the last token, as in CPython (with
    # sep=None it keeps its trailing whitespace). rdb's
    # `action.split('=', 1)`. NOTE calling BOTH forms in one
    # program hits the two-default-shapes contour union (issue 025
    # round-3 notes) -- one form per program.
    r = []
    n = len(self)
    if sep is None:
      i = 0
      while i < n:
        while i < n and (self[i] == " " or self[i] == "\t" or self[i] == "\n" or self[i] == "\r"):
          i += 1
        if i >= n:
          break
        if maxsplit >= 0 and len(r) == maxsplit:
          r.append(self.__pyc_substr__(i, n))
          break
        j = i
        while j < n and not (self[j] == " " or self[j] == "\t" or self[j] == "\n" or self[j] == "\r"):
          j += 1
        r.append(self.__pyc_substr__(i, j))
        i = j
      return r
    m = len(sep)
    if m == 0:
      raise ValueError("empty separator")
    start = 0
    while maxsplit < 0 or len(r) < maxsplit:
      k = self.find(sep, start)
      if k < 0:
        break
      r.append(self.__pyc_substr__(start, k))
      start = k + m
    r.append(self.__pyc_substr__(start, n))
    return r
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
  def endswith(self, suffix):
    n = len(self)
    m = len(suffix)
    if m > n:
      return False
    i = 0
    while i < m:
      if self[n - m + i] != suffix[i]:
        return False
      i += 1
    return True
  def find(self, sub, start=0, end=None):
    # CPython's optional start/end, normalized like a slice (lz2's
    # `c.find(c[fr:to], 0, fr)` resolved to nothing without them, and
    # every value downstream of it went untyped).
    n = len(self)
    i = start
    if i < 0:
      i += n
      if i < 0:
        i = 0
    e = n
    if end is not None:
      e = end
      if e < 0:
        e += n
      if e > n:
        e = n
    return __pyc_c_call__(int, "_CG_str_find", str, self, str, sub, int, i, int, e)
  def index(self, sub, start=0, end=None):
    # str had no .index() at all -- only find() -- so `s.index(x)`
    # fell through to whatever OTHER class's .index() dispatch
    # resolved to (sudoku2.py's `lines[row].index(str(digit))`,
    # `lines[row]` a str, landed in list.index's body treating the
    # string as a list, corrupting the receiver's inferred type
    # program-wide). Unlike list.index()'s "-1 instead of raising"
    # (chosen before issue 011's exception support existed), str.index
    # matches CPython exactly: raises ValueError on a missing
    # substring, since callers rely on catching it (sudoku2.py's
    # `except ValueError: pass` around exactly this call).
    i = self.find(sub, start, end)
    if i < 0:
      raise ValueError("substring not found")
    return i
  def replace(self, old, new):
    # issues/050: one pass, one allocation. Also CPython's empty-`old`
    # case, which the old loop answered with `self` unchanged.
    return __pyc_c_call__(str, "_CG_str_replace", str, self, str, old, str, new)
  def count(self, sub):
    n = len(self)
    m = len(sub)
    if m == 0:
      return n + 1
    c = 0
    i = 0
    while i + m <= n:
      k = 0
      while k < m and self[i + k] == sub[k]:
        k += 1
      if k == m:
        c += 1
        i += m
      else:
        i += 1
    return c
  def isdigit(self):
    n = len(self)
    if n == 0:
      return False
    i = 0
    while i < n:
      o = ord(self[i])
      if o < 48 or o > 57:
        return False
      i += 1
    return True
  def splitlines(self):
    r = []
    n = len(self)
    i = 0
    start = 0
    while i < n:
      if self[i] == "\n" or self[i] == "\r":
        end = i
        if self[i] == "\r" and i + 1 < n and self[i + 1] == "\n":
          i += 1
        i += 1
        r.append(self.__pyc_substr__(start, end))
        start = i
      else:
        i += 1
    if start < n:
      r.append(self.__pyc_substr__(start, n))
    return r
  def rjust(self, width, fillchar=" "):
    pad = width - len(self)
    if pad <= 0:
      return self
    return (fillchar * pad) + self
  def ljust(self, width, fillchar=" "):
    pad = width - len(self)
    if pad <= 0:
      return self
    return self + (fillchar * pad)
  def center(self, width, fillchar=" "):
    pad = width - len(self)
    if pad <= 0:
      return self
    left = pad // 2
    right = pad - left
    return (fillchar * left) + self + (fillchar * right)

