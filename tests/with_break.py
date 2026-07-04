class Trace:
    def __init__(self, name):
        self.name = name
    def __enter__(self):
        print("enter", self.name)
    def __exit__(self, a, b, c):
        print("exit", self.name)

for i in (1, 2):
    print("loop", i)
    with Trace("A"):
        with Trace("B"):
            if i == 1:
                print("breaking")
                break
            print("not breaking")
        print("after B")
    print("after A")
print("done")
