# ifa/172: an int-keyed and an object-keyed dict at module scope. Their
# setters never reached the module-level allocation sites (a global read is
# a folded load with no backward edge), so both stayed on one CreationSet.
class B:
    pass
d = {}
d[3] = 1
b = B()
num = {}
num[b] = 2
print(d[3], num[b])
