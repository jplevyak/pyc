a = [1, 2, 3]
b = [1, 2, 3]
c = [1, 2, 4]
print(hash(tuple(a)) == hash(tuple(b)))
print(hash(tuple(a)) != hash(tuple(c)))

board = [[0, 1, 2], [3, 4, 5]]
seen = {}
for row in board:
    seen[hash(tuple(row))] = 1
print(hash(tuple([0, 1, 2])) in seen)
print(hash(tuple([9, 9, 9])) in seen)
