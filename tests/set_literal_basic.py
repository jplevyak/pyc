s = {1, 2, 3}
print(len(s))

dup = {1, 2, 2, 3, 1}
print(len(dup))

print(2 in s)
print(5 in s)
print(5 not in s)
print(2 not in s)

comp = {y * 2 for y in [1, 2, 3]}
print(len(comp))
print(2 in comp)
print(6 in comp)

evens = {z for z in range(10) if z % 2 == 0}
print(len(evens))

s.add(4)
print(len(s))
s.discard(2)
print(len(s))
print(2 in s)

total = 0
for v in s:
    total += v
print(total)

print(s)

lst = [1, 2, 3]
print(2 in lst)
print(5 in lst)
print(5 not in lst)
