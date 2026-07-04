# issues/002 Case B: a bound-method closure stored into a
# None-initialized global from inside one function and called from
# another. The global's single flow-insensitive cell types as
# SUM{__pyc_None_type__, closure}; codegen must look through the
# nullable SUM to unpack the closure call (closure_fun_type in
# ifa/codegen/codegen_common.cc).
class Counter:
  v = 0
  get = lambda y: y.v

stash = None

def setup():
  global stash
  c = Counter()
  c.v = 42
  stash = c.get

def use():
  print(stash())

def use_guarded():
  if stash is not None:
    print(stash())

setup()
use()
use_guarded()
