# issues/195: a list, dict or set literal as a field default. CPython raises
# ValueError when the class is made; pyc refuses at compile time.
from dataclasses import dataclass


@dataclass
class A:
    x: list = []


print(A())
