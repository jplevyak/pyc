# issues/044: list.__add__ (`+`) must not mutate either operand --
# only list.__iadd__ (`+=`)/append()/extend() are in-place.
a = [1, 2, 3]
b = a + [4, 5]
print(a)
print(b)

# A receiver aliased through an object field must stay independent
# across separate `+` results -- each call built a fresh list, not a
# shared/mutated one.
class node:
    def __init__(self, route):
        self.route = route

    def extend(self, move):
        return node(self.route + [move])

start = node([])
children = [start.extend(i) for i in range(4)]
for c in children:
    print(c.route)
