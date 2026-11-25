# dataclasses.fields, astuple, asdict and replace (issues/216): generated
# per class by the lowering, and recursive into nested dataclasses, lists,
# dicts and tuples as CPython's are.
import dataclasses
from dataclasses import dataclass, field, fields, astuple, asdict, replace


@dataclass
class Point:
    x: float
    y: float = 0.0


@dataclass
class Item:
    name: str
    qty: int = 1
    tags: list = field(default_factory=list, repr=False)
    note: str = field(default="", init=False, compare=False)

    def __post_init__(self):
        self.note = self.name.upper()


@dataclass(frozen=True)
class Seg:
    a: Point
    b: Point
    pair: tuple = (1, "x")


@dataclass
class Point3(Point):
    z: float = 0.0


p = Point(1.5, 2.5)
print([f.name for f in fields(p)], [f.name for f in fields(Point)], fields(p)[0] is fields(Point)[0])
print([(f.name, f.init, f.repr, f.compare, f.hash, f.kw_only) for f in fields(Item)])
print(astuple(p), asdict(p), astuple(Point3(1.0, 2.0, 3.0)), [f.name for f in fields(Point3)])
s = Seg(Point(0.0, 1.0), Point(2.0, 3.0))
print(astuple(s))
it = Item("bolt", 3, ["m4"])
print(astuple(it), it)
t = astuple(it)
t[2].append("copied")
print(it.tags, t[2])
q = replace(p, y=9.0)
print(q, p, q == p, replace(p) == p, replace(p) is p)
print(replace(it, qty=7), dataclasses.replace(s, b=Point(5.0, 5.0)))
print(replace(Point3(1.0, 2.0, 3.0), z=4.0, x=0.5))
ps = [Point(float(i), float(i * i)) for i in range(3)]
print(list(map(astuple, ps)), [asdict(v)["y"] for v in ps])

def err(f):
    try:
        f()
    except TypeError as e:
        print("TypeError", e)
    except ValueError as e:
        print("ValueError", e)

err(lambda: replace(it, note="n"))
err(lambda: replace(p, w=1.0))
