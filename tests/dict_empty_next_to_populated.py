# An empty dict([]) in a program that also builds a populated dict. Once
# ifa/172 stopped merging the two onto one CreationSet, the empty one's
# own contour has a bottom key type, and FA type-checks loops that never
# run over it: `for pair in pairs` in __pyc_dict_from_iterable__ over
# `[]`, and dict.__str__'s `while i < self._len` with _len == 0. The
# ifa/160 family: unreachable code over an empty container.
dd = dict([(1, 2), (3, 4)])
print(dd)
e = dict([])
print(len(e), e)
