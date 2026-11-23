# issues/208: an IntEnum member is an int carrying a name; pyc has no int
# subclass that carries attributes, so it refuses rather than make a member
# that is not an int (isinstance(Prio.LOW, int) is True in CPython).
from enum import IntEnum


class Prio(IntEnum):
    LOW = 1
    HIGH = 2


print(Prio.LOW + 1)
