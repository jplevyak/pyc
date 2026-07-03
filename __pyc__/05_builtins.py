def abs(x):
  if (x < 0):
    return -x
  return x

def all(iterable):
  for element in iterable:
    if not element:
      return False
  return True

def any(iterable):
  for element in iterable:
    if element:
      return True
  return False

def bin(x): # return integer as string in binary
  if x < 0:
    prefix = "-0b"
    x = -x
  else:
    prefix= "0b"
  if x == 0:
    return prefix + "0"
  s = ""
  while (x > 0):
    if (x&1 == 0):
      s = "0" + s
    else:
      s = "1" + s
    x = x >> 1
  return prefix + s

def exit(status = 0):
    __pyc_c_call__(int, "::exit", int, status)

# issues/013: minimal `assert` support -- the frontend (PY_assert_stmt
# in python_ifa_build_if1.cc) lowers `assert cond, msg` to
# `if not cond: __pyc_assert_fail__(msg)`, calling this directly by
# name (no exception model exists yet -- issues/011 -- so this aborts
# the process rather than raising a catchable AssertionError).
def __pyc_assert_fail__(msg):
    if len(msg):
        print("AssertionError: " + msg)
    else:
        print("AssertionError")
    exit(1)

class range:
  i = 0
  j = 0
  s = 1
  def __init__(self, aj):
    self.j = aj
    return self;
  def __init__(self, ai, aj, ak = 1):
    self.i = ai
    self.j = aj
    self.s = ak
    return self
  def __pyc_more__(self):
    if self.s >= 0:
      return self.i < self.j
    else:
      return self.i > self.j
  def __iter__(this):
    return this
  def __next__(self):
    x = self.i
    self.i += self.s
    return x

def len(x):
  return x.__len__()

def chr(x):
    return __pyc_c_call__(str, "_CG_chr", int, x)

def ord(x):
    return __pyc_c_call__(int, "_CG_ord", str, x)

def __hex(x):
  if x < 10:
    return chr(ord('0') + x)
  elif x < 16:
    return chr(ord('a') + x - 10)
  else:
    return __hex(x / 16) + __hex(x % 16)

def hex(x):
  # Mirror bin()'s sign handling (lines 18-21).  Without this,
  # hex(-1) reduces in __hex's `x < 10` branch to
  # chr(ord('0') + -1) = '/', producing "0x/".
  if x < 0:
    return "-0x" + __hex(-x)
  return "0x" + __hex(x)

def __byte_hex2(x):
  if x < 16:
    return "0" + __hex(x)
  return __hex(x)

def isinstance(obj, ci):
  return __pyc_primitive__(__pyc_symbol__("isinstance"), obj, __pyc_clone_constants__(ci))

def issubclass(c1, c2):
  return __pyc_primitive__(__pyc_symbol__("issubclass"), c1, __pyc_clone_constants__(c2))
