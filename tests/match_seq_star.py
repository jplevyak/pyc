# issues/023: star capture in sequence patterns (`case [a, *rest]:`),
# reusing the star_expr/PY_star_expr grammar node issue 024 built for
# extended-unpacking assignment targets -- previously a parse error
# (listmaker/testlist_comp had no star alternative at all).
def head_rest(v):
    match v:
        case [a, *rest]:
            print("head", a, "rest", rest)
        case _:
            print("other")

head_rest([1, 2, 3, 4])
head_rest([1])
head_rest([])
head_rest("nope")

def init_last(v):
    match v:
        case [*init, last]:
            print("init", init, "last", last)
        case _:
            print("other")

init_last([1, 2, 3])
init_last([5])

def first_mid_last(v):
    match v:
        case [first, *mid, last]:
            print("first", first, "mid", mid, "last", last)
        case _:
            print("other")

first_mid_last([1, 2, 3, 4, 5])
first_mid_last([1, 2])
first_mid_last([1])

# `*_` discards, doesn't bind.
def first_only(v):
    match v:
        case [a, *_]:
            print("first is", a)
        case _:
            print("other")

first_only([9, 8, 7])

# Bare `[*rest]` always matches any list/tuple, binding a copy of the
# whole thing.
def all_of(v):
    match v:
        case [*rest]:
            print("all:", rest)

all_of([1, 2, 3])
all_of(())
all_of((1, 2))

# Guard referencing the star capture; must see the binding (guard
# evaluated inside the same nested control flow the capture is made
# in, same rule as ordinary sequence-pattern guards).
def classify(v):
    match v:
        case [a, *rest] if len(rest) > 1:
            print("long rest", a, rest)
        case [a, *rest]:
            print("short rest", a, rest)
        case _:
            print("other")

classify([1, 2, 3, 4])
classify([1, 2])
classify([1])

# Tuple-pattern star form (parenthesized, not bracketed).
def tuple_form(v):
    match v:
        case (a, *rest):
            print("tuple", a, rest)
        case _:
            print("other")

tuple_form((1, 2, 3))
tuple_form([1, 2, 3])

# Nested sequence pattern with a star inside the nested element.
def nested(v):
    match v:
        case [[a, *r], b]:
            print("nested", a, r, b)
        case _:
            print("other")

nested([[1, 2, 3], 99])

# A star capture must bind a FRESH local, not reuse/mutate a
# same-named outer variable (mirrors the analogous positional
# class-pattern shadowing check).
rest = "outer_rest_value"

def shadow_check(v):
    match v:
        case [a, *rest]:
            print("bound rest:", rest)

shadow_check([1, 2, 3])
print("outer still:", rest)
