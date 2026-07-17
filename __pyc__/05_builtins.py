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
# name. issue 011 landed real exception control flow, so this now
# raises a catchable AssertionError(msg) -- forward reference to the
# class (defined later, 08_exception.py) is fine: __pyc__/ is
# concatenated as one module before any function body executes.
def __pyc_assert_fail__(msg):
    raise AssertionError(msg)

class range:
  i = 0
  j = 0
  s = 1
  # __pyc_clone_constants__ on the ctor params does three things via
  # gen_class_pyda + ifa/issues/045: per-constant __new__ contours
  # (the flags propagate to the wrapper formals), per-constant range
  # CSs, AND per-receiver-CS method contours (the class gets
  # clone_methods_per_cs, so __pyc_more__/__next__ split per CS in
  # the PER_CS_RECEIVER precision stage). Together `self.i < self.j`
  # folds per clone: an empty receiver's range(0, 0) loop header
  # folds false and its dead body is never type-checked (issue 040:
  # `k=[]; print(k)` next to a non-empty list used to NOTYPE inside
  # list.__str__'s loop). The flags WITHOUT the method-split stage
  # were measured useless -- see issue 040's trace.
  def __init__(self, aj):
    self.j = __pyc_clone_constants__(aj)
    return self;
  def __init__(self, ai, aj, ak = 1):
    self.i = __pyc_clone_constants__(ai)
    self.j = __pyc_clone_constants__(aj)
    self.s = __pyc_clone_constants__(ak)
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
  def __len__(self):
    # issue 025 R1 "missing sequence ops": reversed(range(n)) needs
    # len()+__getitem__ on range (reversed() is index-based); range
    # had neither, so reversed(range(...)) aborted at runtime
    # ("getter not resolved" -- linalg's first blocker). CPython's
    # exact range-length formula (ceil((j-i)/s) with the sign split
    # so // stays floor division towards the right answer).
    if self.s > 0:
      if self.j <= self.i:
        return 0
      return (self.j - self.i - 1) // self.s + 1
    else:
      if self.j >= self.i:
        return 0
      return (self.i - self.j - 1) // (-self.s) + 1
  def __getitem__(self, idx):
    if idx < 0:
      idx = idx + self.__len__()
    return self.i + idx * self.s
  def __pyc_tolist__(self):
    # list(range(...)) -- see the list() intercept in
    # python_ifa_build_if1.cc (issue 025).
    r = []
    while self.__pyc_more__():
      r.append(self.__next__())
    return r

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
    # // not /: int.__truediv__ is Python-3 true division (float).
    return __hex(x // 16) + __hex(x % 16)

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

def id(x):
  # Identity as int64: the address for heap objects, the value bits
  # for unboxed scalars (diverges from CPython's every-value-is-an-
  # object identity; fine for the id(a) == id(b) idiom -- issue 025:
  # timsort, pylife).
  return __pyc_primitive__(__pyc_symbol__("id"), x)

def hash(x):
  # Dispatches to __hash__ (defined on str and int today -- issue
  # 025: sudoku1/sudoku2). Deterministic across runs, unlike
  # CPython's seeded str hashing; programs only rely on
  # self-consistency within one run.
  return x.__hash__()

def iter(x):
  # issue 025: functools.reduce (pisang) and voronoi2 call these.
  return x.__iter__()

def next(it):
  # pyc iterators' __next__ self-advances (list/base/generator
  # iterators all do), so plain delegation is correct. Calling past
  # exhaustion is undefined (no StopIteration -- issue 011), same as
  # every other pyc iterator.
  return it.__next__()

# ---- issue 025 "has no type" bucket: previously-missing builtins ----
# Pure-Python implementations. Py3 returns lazy iterators from
# zip/map/filter/enumerate/reversed; these return lists (shedskin-
# style divergence) -- equivalent under iteration and list().

def zip(a, b):
  r = []
  n = len(a)
  nb = len(b)
  if nb < n:
    n = nb
  i = 0
  while i < n:
    r.append((a[i], b[i]))
    i = i + 1
  return r

def enumerate(seq):
  r = []
  i = 0
  for x in seq:
    r.append((i, x))
    i = i + 1
  return r

def sum(seq):
  # int seed + float elements resolves to float via the numeric
  # unification (AVar::num_coerce).
  t = 0
  for x in seq:
    t = t + x
  return t

# min/max support both call forms via the default-None + `is None`
# narrowing pattern (each call arity/type gets its own EntrySet
# contour; the dead branch is pruned by nil narrowing).
# min/max: the key-is-None branches keep each call contour
# monomorphic via nil narrowing (same pattern as list.sort in
# 04_sequence.py) -- in a key= contour the keyless comparison branch
# is dead, so elements never need __lt__/__gt__ of their own
# (genetic2's `max(self.population, key=fitness)` was its post-FA
# first blocker: `key` had no formal to bind to, the call didn't
# match, and the untyped result cascaded into a codegen assert).
def min(a, b=None, key=None):
  if b is None:
    if key is None:
      m = a[0]
      for x in a:
        if x < m:
          m = x
      return m
    m = a[0]
    km = key(m)
    for x in a:
      kx = key(x)
      if kx < km:
        m = x
        km = kx
    return m
  if key is None:
    if b < a:
      return b
    return a
  if key(b) < key(a):
    return b
  return a

def max(a, b=None, key=None):
  if b is None:
    if key is None:
      m = a[0]
      for x in a:
        if x > m:
          m = x
      return m
    m = a[0]
    km = key(m)
    for x in a:
      kx = key(x)
      if kx > km:
        m = x
        km = kx
    return m
  if key is None:
    if b > a:
      return b
    return a
  if key(b) > key(a):
    return b
  return a

def reversed(seq):
  r = []
  i = len(seq) - 1
  while i >= 0:
    r.append(seq[i])
    i = i - 1
  return r

def pow(a, b):
  return a ** b

def round(x):
  # Simple form only (round(x) -> int). Uses libc round: half-away-
  # from-zero, a documented divergence from Py3 banker's rounding.
  return int(__pyc_c_call__(float, "round", float, float(x)))

def map(f, seq):
  r = []
  for x in seq:
    r.append(f(x))
  return r

def filter(f, seq):
  r = []
  for x in seq:
    if f(x):
      r.append(x)
  return r

def sorted(seq):
  # NOTE deliberately NO key=/reverse= parameters (use
  # list.sort(key=, reverse=) instead, 04_sequence.py): merely ADDING
  # defaulted params here -- even unused, and regardless of whether
  # the default is inlined or global -- routes sorted() calls through
  # a default_wrapper, and that extra Fun shifted the FA splitter's
  # trajectory enough that builtins_batch's sum() stopped getting
  # per-call-site contours (its int result printed as float bits).
  # The issue 033/040 split-order fragility; revisit when that has a
  # real fix.
  # `<`-only comparison, same contract note as list.sort.
  r = []
  for x in seq:
    r.append(x)
  i = 1
  while i < len(r):
    x = r[i]
    j = i - 1
    while j >= 0 and x < r[j]:
      r[j + 1] = r[j]
      j = j - 1
    r[j + 1] = x
    i = i + 1
  return r

def repr(x):
  return x.__repr__()
