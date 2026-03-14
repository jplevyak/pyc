# Mutable default argument with function
def f(a, L=[]):
    L.append(a)
    return L

print(f(1))
print(f(2))
print(f(3))

# Mutable default argument with lambda
g = lambda a, L=[] : (L.append(a), L)[1]
print(g(1))
print(g(2))
h = g
print(h(3))
