# bisect and operator (issues/207).
import bisect
import operator
from operator import itemgetter
a = [1, 2, 4, 4, 8]
print(bisect.bisect(a, 4), bisect.bisect_left(a, 4), bisect.bisect_right(a, 0), bisect.bisect_left(a, 9), bisect.bisect(a, 4, 0, 2))
bisect.insort(a, 3)
bisect.insort_left(a, 4)
print(a)
recs = [("a", 1), ("b", 3), ("c", 5)]
print(bisect.bisect(recs, 3, key=lambda r: r[1]), bisect.bisect_left(recs, 3, key=lambda r: r[1]))
bisect.insort(recs, ("x", 4), key=lambda r: r[1])
print(recs)
try:
    bisect.bisect(a, 1, -1)
except ValueError as e:
    print("ValueError", e)
print(operator.add(2, 3), operator.mul("ab", 2), operator.sub(5, 7), operator.truediv(7, 2), operator.floordiv(7, 2), operator.mod(7, 3))
print(operator.neg(4), operator.pow(2, 10), operator.lt(1, 2), operator.eq("a", "a"), operator.not_(0), operator.truth([]), operator.contains([1, 2], 2))
print(operator.and_(6, 3), operator.or_(6, 3), operator.xor(6, 3), operator.lshift(1, 4), operator.rshift(16, 2), operator.inv(5), operator.abs(-3))
print(operator.getitem([5, 6], 1), operator.countOf([1, 2, 1], 1), operator.indexOf("abc", "c"), operator.concat([1], [2]))
l = [1, 2, 3]
operator.setitem(l, 0, 9)
operator.delitem(l, -1)
print(l, operator.iadd([1], [2]), operator.is_(None, None))
print(sorted([("b", 2), ("a", 9), ("c", 1)], key=itemgetter(1)), itemgetter(0, 2)("xyz"), itemgetter(1)("xyz"))
from functools import reduce
print(reduce(operator.mul, [1, 2, 3, 4]), list(map(operator.neg, [1, 2])))
