# Assigning another module's global, `m.G = v`, is a store to that module
# variable. The frontend left the target unresolved and the compiler aborted
# (`if1_move` asserted) -- othello2_check's `othello2.NODES = 0`.
import module_attribute_store_helper as h

h.COUNT = 5
h.bump()
print(h.COUNT)
h.COUNT += 10
print(h.COUNT)

def set_from_function():
    h.COUNT = 100

set_from_function()
h.bump()
print(h.COUNT)

h.NODES = 7
h.visit()
print(h.NODES)
h.reset_nodes()
h.visit()
print(h.NODES)
