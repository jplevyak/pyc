# issues/183: `x + 5` with x {None, int} must raise TypeError when x is None,
# or be refused. pyc compiled it in every mode and read None as 0.
import sys
x = None if len(sys.argv) > 5 else 1
y = 1 if len(sys.argv) > 5 else None
print(x + 5)
try:
    print(y + 5)
except TypeError:
    print("TypeError")
