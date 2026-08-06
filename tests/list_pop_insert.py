l = [1, 2, 3, 4, 5]
print(l.pop())
print(l)
print(l.pop(0))
print(l)
print(l.pop(-2))
print(l)

l2 = [10, 20, 30]
l2.insert(0, 1)
print(l2)
l2.insert(len(l2), 99)
print(l2)
l2.insert(2, 15)
print(l2)
l2.insert(-1, 88)
print(l2)
l2.insert(100, 77)
print(l2)
l2.insert(-100, -1)
print(l2)

l3 = []
l3.insert(0, "x")
print(l3)
print(l3.pop())
print(l3)
