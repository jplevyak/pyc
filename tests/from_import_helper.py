# Helper module for tests/from_import.py (marked .ignore so the
# harness doesn't compile it standalone).
GREETING = "hi"

def add(a, b):
    return a + b

def triple(x):
    return x * 3

class Point:
    def __init__(self, x):
        self.x = x

    def doubled(self):
        return self.x * 2
