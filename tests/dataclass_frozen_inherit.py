# issues/195: frozen dataclasses (a field store raises FrozenInstanceError;
# hash is the fields'), a dataclass base, eq=False inheriting the base's
# __eq__, field(init=False) with a default and a factory, __post_init__, and
# typing's TYPE_CHECKING and TypeAlias, which CPython never runs or never reads.
from __future__ import annotations
import dataclasses
from dataclasses import FrozenInstanceError
import typing
from typing import TYPE_CHECKING, TypeAlias

if TYPE_CHECKING:
    from nowhere import Thing                 # never imported
else:
    Thing = int

Pair: TypeAlias = tuple[int, str]


@dataclasses.dataclass(frozen=True)
class Key:
    a: int
    b: str = "z"


@dataclasses.dataclass
class Base:
    x: int
    y: int = 1

    def __post_init__(self):
        self.s = self.x + self.y


@dataclasses.dataclass(eq=False)
class Derived(Base):
    z: int = 5
    log: typing.List[str] = dataclasses.field(default_factory=list, init=False)
    n: int = dataclasses.field(default=9, init=False)


k = Key(1)
print(k, k == Key(1, "z"), hash(k) == hash(Key(1)), k.b, len({Key(1), Key(1), Key(2)}))
try:
    k.a = 2
except FrozenInstanceError as e:
    print("FrozenInstanceError", e)
except AttributeError:
    print("not reached")
print(k.a)
d = Derived(2)
d.log.append("hi")
print(d, d.s, d.log, d.n, d == Derived(2), d == d)
print(Base(1) == Base(1), Base(1) == Derived(1), Thing(3))
