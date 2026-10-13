# ifa/156: numeric_member_split_by_caller's shape, iterated with
# `enumerate` instead of indexed. Both functions' lists then stay in ONE list
# contour (a creation point in each), and the walk out of `c.x = i * 2` reads
# that contour's element and, by CreationSet alone, comes back through both
# functions' `append`s. Separating them needs the OBJECT the read came from:
# which creation point of the list contour, under which call context. CPython
# prints the centres' ints; pyc must not widen them to `0.0`, `2.0`, ...
class Point:
    pass


def make_points(n):
    points = [Point() for _ in range(n)]
    for i, p in enumerate(points):
        p.x = i * 1.5
    return points


def make_centres(k):
    cs = [Point() for _ in range(k)]
    for i, c in enumerate(cs):
        c.x = i * 2
    return cs


pts = make_points(4)
cc = make_centres(3)
for p in pts:
    print(p.x)
for c in cc:
    print(c.x)
