# ifa/191's other half: a None initial value that IS read before the
# overwrite (here by a method __init__ calls) stays in the field's type, so
# the {None, int} operand is refused as before. CPython prints None, then 6.
class A:
    def __init__(self):
        self.x = None
        self.show()
        self.x = 5

    def show(self):
        print(self.x)

a = A()
print(a.x + 1)
