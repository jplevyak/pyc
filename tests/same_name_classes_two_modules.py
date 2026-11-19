# ifa/130 A3: two unrelated classes named `Node`, in two modules, reaching
# one dispatch site and one isinstance. A class tag was named
# `_CG_type_<name>`, one per NAME, so both shared a tag: isinstance(x,
# a.Node) was True for a b.Node, and the reverse. Tags are now one per
# class family (a class and its clones).
import same_name_class_a as a
import same_name_class_b as b
xs = [a.Node(), b.Node(), a.Node()]
for x in xs:
    print(x.f(), isinstance(x, a.Node), isinstance(x, b.Node))
