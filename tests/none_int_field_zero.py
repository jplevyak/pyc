# issues/052 (LLVM) and issues/048 (C): a None-initialised field later
# holding the int ZERO. The union {nil, int64} has no scalar
# representation that can tell None from 0, and the two backends handle
# that differently -- neither correctly:
#
#   CPython  0 2
#   LLVM     None 2      <- SILENTLY WRONG (issues/052)
#   C        aborts      <- refuses, deliberately (issues/048)
#
# The C backend's refusal is the RIGHT behaviour of the two: cg.cc vetoes
# a nil test on a scalar-typed operand precisely because it would render 0
# as "None", which is what LLVM then does.
class V:
    def __init__(self):
        self.a = None
        self.b = None

v = V()
v.a = 0
v.b = 2
print(v.a, v.b)
