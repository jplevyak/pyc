__pyc_declare__ = None

# Identity decorator — `@pyc_struct` is a pyc-side opt-in for POD /
# value-type record codegen (sets `Sym::is_value_type`).  CPython
# has no equivalent, so on the Python side it's a no-op.
def pyc_struct(cls):
  return cls
