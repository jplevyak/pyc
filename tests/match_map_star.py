# issues/023: `**rest` in mapping patterns (`case {"k": v, **rest}:`),
# reusing a new dict_rest_arg grammar node (python.g) -- previously a
# parse error (dictorsetmaker had no '**' NAME alternative at all).
def head_rest(v):
    match v:
        case {"a": x, **rest}:
            print("a=", x, "rest=", rest)
        case _:
            print("other")

head_rest({"a": 1, "b": 2, "c": 3})
head_rest({"a": 1})
head_rest({"b": 2})
head_rest("nope")

# Bare `{**rest}` always matches any mapping, binding a copy of the
# whole thing.
def all_of(v):
    match v:
        case {**rest}:
            print("all:", rest)

all_of({"x": 1, "y": 2})
all_of({})

# Multiple explicit keys plus rest.
def three_keys(v):
    match v:
        case {"a": x, "b": y, **rest}:
            print(x, y, rest)
        case _:
            print("other")

three_keys({"a": 1, "b": 2, "c": 3, "d": 4})
three_keys({"a": 1, "b": 2})

# Guard referencing the rest capture; must see the binding (guard
# evaluated inside the same nested control flow the capture is made
# in, same rule as ordinary mapping-pattern guards).
def classify(v):
    match v:
        case {"a": x, **rest} if len(rest) > 1:
            print("big rest", x, rest)
        case {"a": x, **rest}:
            print("small rest", x, rest)
        case _:
            print("other")

classify({"a": 1, "b": 2, "c": 3})
classify({"a": 1, "b": 2})
classify({"a": 1})

# Nested: a mapping pattern with rest inside a sequence pattern.
def nested(v):
    match v:
        case [{"a": x, **rest}, y]:
            print("nested", x, rest, y)
        case _:
            print("other")

nested([{"a": 1, "b": 2}, 99])

# A `**rest` capture must bind a FRESH local, not reuse/mutate a
# same-named outer variable (mirrors the analogous sequence-pattern
# star-capture and positional class-pattern shadowing checks).
rest = "outer_rest_value"

def shadow_check(v):
    match v:
        case {"a": x, **rest}:
            print("bound rest:", rest, "x:", x)

shadow_check({"a": 1, "b": 2})
print("outer still:", rest)
