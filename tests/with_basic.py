class Trace:
    def __init__(self, name):
        self.name = name

    def __enter__(self):
        print("enter", self.name)
        return self.name + "_val"

    def __exit__(self, a, b, c):
        print("exit", self.name)
        return False

print("test 1")
with Trace("A") as a:
    print("body 1", a)

print("test 2")
with Trace("B") as b, Trace("C") as c:
    print("body 2", b, c)

print("test 3")
with Trace("D"):
    print("body 3")
