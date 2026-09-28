# enumerate is a lazy iterator over the live sequence, as in CPython.
# A loop that deletes from the list it enumerates skips the element after
# each deletion; an eager snapshot visits (and deletes) a different set.
# Distilled from shedskin_examples/chull's Hull.CleanVertices, which
# segfaulted when pyc's enumerate built a list up front.
class V:
    def __init__(self, n, m):
        self.n = n
        self.m = m

vs = [V(0, True), V(1, True), V(2, False), V(3, True), V(4, True), V(5, False)]
for i, v in enumerate(vs):
    if v.m:
        del vs[i]
print([v.n for v in vs])

xs = [10, 20, 30]
for i, x in enumerate(xs):
    if i == 0:
        xs.append(40)
    print(i, x)

print(list(enumerate("ab")))
for i, c in enumerate("xyz"):
    print(i, c)
