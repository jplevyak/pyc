g = lambda a, L=[] : (L.append(a), L)[1]
print g(1)
print g(2)
h = g
print h(3)
