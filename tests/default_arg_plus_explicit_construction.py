# ifa/issues/088: LLVM backend segfaulted constructing a class two
# ways in the same program -- once relying on a None default arg
# (route or [] picks the fallback), once passing an explicit value --
# when the shared __init__ needs a runtime `route or []` truthiness
# check over a None|list receiver. Root cause: the polymorphic
# dispatch classifier had nowhere to put a candidate whose receiver
# type carries no runtime classtag at all (list, like int/float/bool/
# tuple, is a raw runtime layout, never a tagged struct) once the
# nil-receiver case was excluded -- fixed by giving cg_emit_llvm.cc
# the same "exactly one untagged candidate" fallback bucket cg.cc
# already had (issues/025, genetic2's crossover).
class node:
    def __init__(self, route=None):
        self.route = route or []

start = node()
n2 = node([5])
print(start.route)
print(n2.route)
