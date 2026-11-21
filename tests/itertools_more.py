# itertools beyond count and product, as CPython's documented equivalents (issues/207).
import itertools
import sys
from itertools import (chain, islice, repeat, cycle, accumulate, groupby, zip_longest, pairwise,
                       batched, compress, dropwhile, takewhile, filterfalse, starmap, permutations,
                       combinations, combinations_with_replacement, count)
print(list(chain([1, 2], [3], [])), list(chain.from_iterable([[1], [2, 3]])), list(chain("ab", "cd")))
print(list(islice(range(10), 3)), list(islice(range(10), 2, 8, 3)), list(islice(range(10), 2, None)),
      list(islice(count(5), 4)), list(islice(range(10), 5, 2)))
it = iter(range(10))
print(list(islice(it, 2, 4)), list(it))
it = iter(range(10))
print(list(islice(it, 6, 3)), list(it))
print(list(repeat("x", 3)), list(islice(repeat(7), 2)), list(islice(cycle([1, 2]), 5)))
print(list(accumulate([1, 2, 3, 4])), list(accumulate([3, 1, 4], max)), list(accumulate([1, 2], initial=10)),
      list(accumulate(["a", "b"])))
print([(k, list(g)) for k, g in groupby("aaabccaa")], [(k, list(g)) for k, g in groupby([1, 3, 2, 4, 5], lambda x: x % 2)])
gs = list(groupby("aabb"))
print([(k, list(g)) for k, g in gs])
for k, g in groupby("xxyyz"):
    print(k, next(g))
print(list(zip_longest([1, 2, 3], [4], fillvalue=0)), list(zip_longest("ab", "xyz", fillvalue="-")))
print(list(pairwise("abcd")), list(pairwise([1])), list(batched(range(7), 3)), list(compress("abcdef", [1, 0, 1, 1])))
print(list(dropwhile(lambda x: x < 3, [1, 2, 5, 1])), list(takewhile(lambda x: x < 3, [1, 2, 5, 1])),
      list(filterfalse(lambda x: x % 2, range(6))), list(filterfalse(None, [0, 1, 0, 2])))
print(list(starmap(pow, [(2, 3), (3, 2)])))
print(list(permutations([1, 2, 3])), list(permutations("abc", 2)), list(permutations([1], 2)))
print(list(combinations(range(4), 2)), list(combinations("abc", 0)), list(combinations([1], 3)))
print(list(combinations_with_replacement("ab", 2)), list(combinations_with_replacement([1, 2, 3], 1)))
print(list(itertools.product([1, 2], "ab")), list(islice(count(1, 2), 3)))
try:
    print(list(islice([1], -1)))
except ValueError as e:
    print("ValueError", e)
try:
    list(batched([1], len(sys.argv) - 1))
except ValueError as e:
    print("ValueError", e)
