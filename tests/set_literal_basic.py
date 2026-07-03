s = {1, 2, 3}
dup = {1, 2, 2, 3, 1}
print(len(s))
print(len(dup))

print(2 in s)
print(5 in s)
print(5 not in s)
print(2 not in s)

total = 0
for v in s:
    total += v
print(total)

print(s)

lst = [1, 2, 3]
print(2 in lst)
print(5 in lst)
print(5 not in lst)
