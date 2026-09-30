# issues/128 step 2: a field a write "discovers" through a TRANSIENT union is
# withdrawn once the converged types no longer write it. On pass 0 `xs` and
# `ys` share one list contour, so `xs[0]` is {A, B} and both writes land on
# both classes; after the lists separate, A never receives `b` and B never
# receives `a`. The .env sets IFA_DBG_RETRACT, and the .check pins the two
# retractions: without them A and B each carried the other's field, which on
# chull put two unrelated classes' fields at colliding offsets.
class A:
    def __init__(self):
        self.a = 1
class B:
    def __init__(self):
        self.b = 2
xs = [A(), A()]
ys = [B(), B()]
xs[0].a = 5
ys[0].b = 6
print(xs[0].a)
print(ys[0].b)
