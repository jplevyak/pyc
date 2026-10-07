# issues/175: an exception raised inside an operator method must reach the
# caller's handler, as CPython's does. pyc emits no exception check after
# an operator send, so the exception stays pending past the expression.
class V:
    def __add__(self, o):
        raise ValueError("no add")


try:
    print(V() + V())
except ValueError:
    print("caught add")
try:
    print(b'%c' % 300)
except OverflowError:
    print("caught overflow")
print("end")
