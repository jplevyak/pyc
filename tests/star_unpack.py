# issue 024 (PEP 3132): extended iterable unpacking assignment
# targets -- `*name` inside a tuple/list assignment target binds a
# NEW list (always a list, even when the source is a tuple/other
# sequence) holding everything not claimed by the other targets.
class Box:
    def __init__(self):
        self.items = []

a, *b = [1, 2, 3]
print(a, b)

*c, d = [1, 2, 3]
print(c, d)

e, *f, g = [1, 2, 3, 4, 5]
print(e, f, g)

h, *i = (10, 20, 30)
print(h, i)

l, *m, n = [10, 20, 30, 40]
print(l, m, n)

box = Box()
p, *box.items = [1, 2, 3]
print(p, box.items)
