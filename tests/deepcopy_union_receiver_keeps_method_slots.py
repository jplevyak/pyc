# genetic2 under -b: a __deepcopy__ receiver is a union of the original's
# and the copies' CreationSets, so its copy has no compile-time size. The
# LLVM backend sized it from the union's struct (16 of 168 bytes), so the
# copy's method slots were NULL, and `"%s" % self.args` dispatching
# __str__ on a {None, Node} element called address 0. It now copies the
# allocation's run-time size (_CG_prim_copy_any), as the C backend does.
import copy

class Node:
    def __init__(self, op, value, args=None):
        self.op = op
        self.value = value
        self.args = args

    def eval(self, x):
        if self.op == 0:
            return x[self.value]
        if self.op == 1:
            return not self.args[0].eval(x)
        if self.op == 2:
            return self.args[0].eval(x) and self.args[1].eval(x)
        return self.args[0].eval(x) or self.args[1].eval(x)

    def size(self):
        if self.args is None:
            return 1
        return 1 + sum([a.size() for a in self.args])

    def __str__(self):
        if self.op == 0:
            return "(bit %s)" % self.value
        if self.op == 1:
            return "(not %s)" % self.args
        if self.op == 2:
            return "(and %s %s)" % self.args
        return "(or %s %s)" % self.args

def swap(a, b):
    a2 = copy.deepcopy(a)
    b2 = copy.deepcopy(b)
    a2.args = tuple([b2.args[0], a2.args[len(a2.args) - 1]])
    return a2

t = Node(2, 0, (Node(0, 1), Node(3, 0, (Node(1, 0, (Node(0, 2),)), Node(0, 3)))))
u = Node(3, 0, (Node(0, 0), Node(0, 1)))
w = Node(1, 0, (None,))
pop = [t, u]
for i in range(4):
    pop.append(swap(pop[-1], pop[-2]))
best = pop[-1]
print(best, best.size(), best.eval([True, False, True, False]))
print(copy.deepcopy(w), copy.deepcopy(best))
