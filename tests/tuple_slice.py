# ifa/issues/109: tuple slicing. `class tuple` had no __pyc_getslice__ at
# all -- only `class list` did -- so any tuple slice aborted at runtime
# with "list index type mismatch".
#
# The fix is the answer to "lists can represent unknown arity with some
# element type, why can't tuple?": they can. `__pyc_getslice__` reads
# `sizeof_element`, which POPULATES the tuple's generic element, which
# makes tuple_able() false, which makes clone.cc give that CreationSet
# LIST layout -- unknown arity, known element type. The only thing
# missing was that sym_tuple had no element sym to populate (PYC_TUPELEM,
# now on by default).
import sys
t = (1, 2, 3, 4)
print(t[0:2])
print(t[1:])
print(t[:3])
print(t[:])
n = len(sys.argv)          # runtime bound, so the arity is not static
print(t[0:n])
print(t[n:])
s = ("a", "b", "c")
print(s[0:2])
