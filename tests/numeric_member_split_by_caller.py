# ifa/156: one constructor call site, in a contour two callers share, makes
# objects whose `x` is only ever an int in one caller's objects and only
# ever a float in the other's. kmeanspp's shape: points and cluster centres
# are both `[Point() for ...]`, and both lists are filled by one `append`
# contour, whose return feeds each comprehension's accumulator. CPython
# prints the centres' ints and the points' floats; pyc must split the
# Point contour by caller rather than widen the ints to `0.0`, `2.0`, ...
class Point:
    pass


def make_points(n):
    points = [Point() for _ in range(n)]
    for i in range(n):
        points[i].x = i * 1.5
    return points


def make_centres(k):
    cs = [Point() for _ in range(k)]
    for i in range(k):
        cs[i].x = i * 2
    return cs


pts = make_points(4)
cc = make_centres(3)
for i in range(4):
    print(pts[i].x)
for i in range(3):
    print(cc[i].x)
