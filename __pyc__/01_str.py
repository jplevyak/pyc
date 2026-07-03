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
  def __mul__(self, l):
    return __pyc_c_call__(str, "_CG_string_mult", str, self, int, l)
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
