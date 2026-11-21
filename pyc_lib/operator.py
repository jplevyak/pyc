# pyc shim for the standard `operator` module (issues/207): each function
# is the operator it names, so it dispatches as the operator does.
# attrgetter and methodcaller read attributes by a name given at run time,
# which pyc does not do (a compile error: no such name here).

def lt(a, b): return a < b
def le(a, b): return a <= b
def eq(a, b): return a == b
def ne(a, b): return a != b
def ge(a, b): return a >= b
def gt(a, b): return a > b
def not_(a): return not a
def truth(a): return True if a else False
def is_(a, b): return a is b
def is_not(a, b): return a is not b
def abs(a): return a.__abs__()
def add(a, b): return a + b
def and_(a, b): return a & b
def floordiv(a, b): return a // b
def index(a): return a.__index__()
def inv(a): return ~a
invert = inv
def lshift(a, b): return a << b
def mod(a, b): return a % b
def mul(a, b): return a * b
def neg(a): return -a
def or_(a, b): return a | b
def pos(a): return +a
def pow(a, b): return a ** b
def rshift(a, b): return a >> b
def sub(a, b): return a - b
def truediv(a, b): return a / b
def xor(a, b): return a ^ b
def concat(a, b): return a + b
def contains(a, b): return b in a


def countOf(a, b):
    n = 0
    for x in a:
        if x is b or x == b:
            n += 1
    return n


def indexOf(a, b):
    i = 0
    for x in a:
        if x is b or x == b:
            return i
        i += 1
    raise ValueError('sequence.index(x): x not in sequence')


def getitem(a, b): return a[b]


def setitem(a, b, c):
    a[b] = c


def delitem(a, b):
    del a[b]


def iadd(a, b):
    a += b
    return a


def isub(a, b):
    a -= b
    return a


def imul(a, b):
    a *= b
    return a


def iconcat(a, b):
    a += b
    return a


# itemgetter(i) returns obj[i]; itemgetter(i, j, ...) the tuple of them.
class itemgetter:
    def __init__(self, *items):
        self._items = items

    def __call__(self, obj):
        if len(self._items) == 1:
            return obj[self._items[0]]
        return tuple([obj[i] for i in self._items])
