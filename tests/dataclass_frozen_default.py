# A frozen dataclass field with a default: its class attribute is stored
# when the class is made, not through the frozen setter, which raised
# "cannot assign to field" there and left it pending to the next check
# (issues/216).
from dataclasses import dataclass, FrozenInstanceError


@dataclass(frozen=True)
class Seg:
    pair: int = 1
    name: str = "s"


def f(n):
    if n > 5:
        raise ValueError("x")
    return n


try:
    print(f(1))
except ValueError:
    print("caught")
s = Seg()
print(s, Seg.pair, Seg.name, s == Seg(1, "s"))
try:
    s.pair = 2
except FrozenInstanceError as e:
    print("FrozenInstanceError", e)
