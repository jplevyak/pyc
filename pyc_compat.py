import functools

__pyc_declare__ = None

# Identity decorator — `@pyc_struct` is a pyc-side opt-in for POD /
# value-type record codegen (sets `Sym::is_value_type`).  CPython
# has no equivalent, so on the Python side it's a no-op.
def pyc_struct(cls):
  return cls


# @pyc_compare (issue 068): pyc derives the record comparison family
# (__eq__, __lt__, and the delegated __ne__/__gt__/__le__/__ge__) as
# field-folds.  On the Python side, provide the matching semantics so
# CPython VERIFY agrees: field-wise __eq__ and lexicographic __lt__ over
# the fields in definition order (instance-dict order), with
# functools.total_ordering filling in the rest -- equivalent to
# dataclass(order=True) for a totally-ordered record.
def pyc_compare(cls):
  def __eq__(self, other):
    return self.__dict__ == other.__dict__
  def __lt__(self, other):
    return list(self.__dict__.values()) < list(other.__dict__.values())
  cls.__eq__ = __eq__
  cls.__lt__ = __lt__
  return functools.total_ordering(cls)
