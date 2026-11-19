# ifa/102 step 3: a classtag dispatch whose receiver union holds None, where
# None has no such method. CPython raises AttributeError at the call. The
# dispatch had no None test, so it read the classtag through a null
# pointer: a segfault, or a call through whatever the slot read found.
# Codegen now checks that every member of the receiver's union has an arm
# (poly_dispatch_coverage). An unclaimed None gets the None check a direct
# call has (ifa/165); anything else unclaimed is a compile refusal.
class A:
    def m(self):
        return 1

class B:
    def m(self):
        return 2

def pick(i):
    if i == 0:
        return A()
    if i == 1:
        return B()
    return None

for i in range(3):
    print(pick(i).m())
print("not reached")
