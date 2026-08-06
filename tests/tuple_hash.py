a = (1, 2, 3)
b = (1, 2, 3)
c = (1, 2, 4)
print(hash(a) == hash(b))
print(hash(a) == hash(a))
print(hash(a) != hash(c))

seen = {}
seen[hash(a)] = 1
print(hash(b) in seen)
print(hash(c) in seen)

print(hash(()) == hash(()))
print(hash((1,)) != hash((2,)))
