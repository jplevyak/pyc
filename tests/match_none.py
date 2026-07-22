# issues/023 + ifa/issues/060: `case None:` combined with narrowing or
# capturing patterns in the same match statement. This once hit a
# code-generation limitation (None coerced into a shared scalar clone
# as (scalar)NULL, so the None arm matched a falsy scalar) and was
# refused at compile time. Fixed by splitting None into its own contour
# in type_cannonicalize (ifa/issues/060 mechanism 2); all combinations
# below now match CPython.

# None + literal + wildcard
def with_literal(val):
    match val:
        case None:
            print("lit none")
        case 5:
            print("lit five")
        case _:
            print("lit other")

with_literal(None)
with_literal(5)
with_literal(9)


# None + True/False (bool singletons)
def with_bool(val):
    match val:
        case None:
            print("bool none")
        case True:
            print("bool true")
        case False:
            print("bool false")

with_bool(None)
with_bool(True)
with_bool(False)


# None + sequence pattern + capture
def with_seq(val):
    match val:
        case None:
            print("seq none")
        case [a, b]:
            print("seq pair", a, b)
        case x:
            print("seq cap", x)

with_seq(None)
with_seq([1, 2])
with_seq(7)


# None + bare capture
def with_capture(val):
    match val:
        case None:
            print("cap none")
        case y:
            print("cap", y)

with_capture(None)
with_capture(42)
