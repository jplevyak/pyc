__pyc_declare__ = None

# Identity decorator — `@pyc_struct` is a pyc-side opt-in for POD /
# value-type record codegen (sets `Sym::is_value_type`).  CPython
# has no equivalent, so on the Python side it's a no-op.
def pyc_struct(cls):
  return cls


# @pyc_compare (issue 068): pyc derives a field-wise __eq__ for the record
# (the class side of the derive / field-fold framework).  On the Python
# side, provide the matching semantics so CPython VERIFY agrees -- two
# instances are equal iff their fields (instance dicts) are equal.
def pyc_compare(cls):
  def __eq__(self, other):
    return self.__dict__ == other.__dict__
  cls.__eq__ = __eq__
  return cls
