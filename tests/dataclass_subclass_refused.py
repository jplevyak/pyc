# issues/195: a subclass of a dataclass that is not one itself would inherit
# __repr__ and __eq__ written for the base's class. CPython prints `B(x=1)`;
# pyc refuses rather than print `A(x=1)`.
from dataclasses import dataclass


@dataclass
class A:
    x: int


class B(A):
    pass


print(B(1))
