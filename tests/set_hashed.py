# issues/118: set is a hash index over insertion-ordered storage. A large
# set, discard-then-add (the old shift delete lost the next add), pop,
# and set([]). One element type per program: two sets of different
# element types in one program is ifa/172.
s = {1, 2, 3}
s.discard(1)
s.add(9)
print(9 in s, len(s), sorted(s))
t = set()
for i in range(20000):
    t.add(i * 7)
n = 0
for i in range(20000):
    if i in t:
        n += 1
print(len(t), n)
while t:
    t.pop()
    if len(t) < 19990:
        break
print(len(t))
e = set([])
print(len(e), 3 in e)
