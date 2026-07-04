class Trace:
    def __enter__(self):
        print("enter")
    def __exit__(self, a, b, c):
        print("exit")

def foo():
    with Trace():
        print("body")
        return 42

print(foo())
