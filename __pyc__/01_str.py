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
