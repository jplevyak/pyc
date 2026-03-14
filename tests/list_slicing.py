# Slice read
a = [1, 2, 3, 4]
print(a)
b = a[2:4]
print(b)

# Slice assignment
a = [1, 2, 3, 4]
print(a)
b = a[2:]
c = a
a[2:] = [6, 7, 8]
print(a)
print(b)
print(c)

# Slice with step
a = [1, 2, 3]
b = a[1::2]
print(b)
b[0] = 4
print(b)
print(a)
