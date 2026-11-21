# abs() calls __abs__; abs(-0.0) is 0.0 (issues/207).
class V:
    def __init__(self, x):
        self.x = x
    def __abs__(self):
        return V(abs(self.x))
    def __repr__(self):
        return "V(" + repr(self.x) + ")"
print(abs(-3), abs(3), abs(-0.0), abs(-2.5), abs(True), abs(False), abs(V(-4)), abs(-7 // 2))
