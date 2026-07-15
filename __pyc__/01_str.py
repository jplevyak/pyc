class str:
  def __add__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("::"), x)
  def __iadd__(self, x):
    return __pyc_operator__(self, __pyc_symbol__("::"), x)
  def __str__(self):
    return __pyc_clone_constants__(self)
  def __repr__(self):
    return "'" + __pyc_clone_constants__(self) + "'"
  def __getitem__(self, key):
    return __pyc_primitive__(__pyc_symbol__("index_object"), self, key)
  def __len__(self):
    return __pyc_primitive__(__pyc_symbol__("len"), self)
  def __iter__(self):
    return __base_iter__(self)
  def __pyc_tolist__(self):
    # list("abc") -> ["a", "b", "c"] -- see the list() intercept in
    # python_ifa_build_if1.cc (issue 025; block.py's first blocker).
    r = []
    for c in self:
      r.append(c)
    return r
  def __mul__(self, l):
    return __pyc_c_call__(str, "_CG_string_mult", str, self, int, l)
  def __hash__(self):
    return __pyc_c_call__(int, "_CG_str_hash", str, self)
  def __eq__(self, x):
    return __pyc_c_call__(bool, "_CG_str_eq", str, self, str, x)
  def __ne__(self, x):
    return __pyc_c_call__(bool, "_CG_str_ne", str, self, str, x)
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
  def __format__(self, spec):
    # issues/006: PEP 3101 format-spec mini-language, see int.__format__.
    return __pyc_c_call__(str, "_CG_format_str_spec", str, self, str, spec)
  def join(self, seq):
    r = ""
    first = True
    for x in seq:
      if not first:
        r = r + self
      r = r + x
      first = False
    return r
  def lower(self):
    r = ""
    for c in self:
      o = ord(c)
      if o >= 65 and o <= 90:
        r = r + chr(o + 32)
      else:
        r = r + c
    return r
  def upper(self):
    r = ""
    for c in self:
      o = ord(c)
      if o >= 97 and o <= 122:
        r = r + chr(o - 32)
      else:
        r = r + c
    return r
  def __contains__(self, x):
    # Substring search by char compare; str has no working slice path
    # yet (the __pyc_any_type__ fallback mis-routes slices of str into
    # index_object), so only __getitem__(int) and __eq__ are used.
    n = len(self)
    m = len(x)
    if m == 0:
      return True
    i = 0
    while i + m <= n:
      j = 0
      while j < m and self[i + j] == x[j]:
        j += 1
      if j == m:
        return True
      i += 1
    return False
  def __pyc_substr__(self, i, j):
    # self[i:j] by char concat -- same no-slice-path caveat as
    # __contains__ above. O(j-i) concats; fine for corpus-scale
    # correctness, optimize via a _CG helper if it ever matters.
    r = ""
    k = i
    while k < j:
      r = r + self[k]
      k += 1
    return r
  def strip(self):
    # Whitespace-only form (no chars argument -- the corpus's one
    # `.strip(x)` call stays unsupported for now).
    n = len(self)
    i = 0
    while i < n and (self[i] == " " or self[i] == "\t" or self[i] == "\n" or self[i] == "\r"):
      i += 1
    j = n
    while j > i and (self[j - 1] == " " or self[j - 1] == "\t" or self[j - 1] == "\n" or self[j - 1] == "\r"):
      j -= 1
    return self.__pyc_substr__(i, j)
  def split(self, sep=None):
    # sep=None: runs of whitespace, no empty tokens (Python
    # semantics). String sep: split on every occurrence, empty
    # tokens included. No maxsplit. NOTE calling BOTH forms in one
    # program hits the two-default-shapes contour union (issue 025
    # round-3 notes) -- one form per program.
    r = []
    n = len(self)
    if sep is None:
      i = 0
      while i < n:
        while i < n and (self[i] == " " or self[i] == "\t" or self[i] == "\n" or self[i] == "\r"):
          i += 1
        j = i
        while j < n and not (self[j] == " " or self[j] == "\t" or self[j] == "\n" or self[j] == "\r"):
          j += 1
        if j > i:
          r.append(self.__pyc_substr__(i, j))
        i = j
      return r
    m = len(sep)
    if m == 0:
      r.append(self)
      return r
    i = 0
    start = 0
    while i + m <= n:
      k = 0
      while k < m and self[i + k] == sep[k]:
        k += 1
      if k == m:
        r.append(self.__pyc_substr__(start, i))
        i += m
        start = i
      else:
        i += 1
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
  def find(self, sub):
    n = len(self)
    m = len(sub)
    i = 0
    while i + m <= n:
      j = 0
      while j < m and self[i + j] == sub[j]:
        j += 1
      if j == m:
        return i
      i += 1
    return -1
  def replace(self, old, new):
    n = len(self)
    m = len(old)
    if m == 0:
      return self
    r = ""
    i = 0
    while i < n:
      k = 0
      if i + m <= n:
        while k < m and self[i + k] == old[k]:
          k += 1
      if k == m:
        r = r + new
        i += m
      else:
        r = r + self[i]
        i += 1
    return r
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
