# list.index with start/stop and ValueError (amaze's distances2.index(dist, idx+1)).
a = [1.5, 2.5, 1.5, 3.5]
i = a.index(1.5)
print(i)
print(a.index(1.5, i + 1))
print(a.index(3.5, -1))
print(a.index(2.5, 0, 2))
print(a.index(1.5, -100, 100))
try:
    a.index(2.5, 2)
except ValueError:
    print("absent after 2")
try:
    a.index(3.5, 0, -1)
except ValueError:
    print("absent before -1")
w = ["x", "y"]
print(w.index("y"))
