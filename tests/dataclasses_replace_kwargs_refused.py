# dataclasses.replace(obj, **changes) is lowered per set of keyword names;
# a '**' dict has no names at compile time (issues/216).
from dataclasses import dataclass, replace


@dataclass
class P:
    x: int
    y: int = 0


changes = {"y": 2}
print(replace(P(1), **changes))
