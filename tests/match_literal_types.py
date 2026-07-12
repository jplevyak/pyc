# Literal patterns of different types mixed in one match statement --
# each needs isinstance-narrowing before the __eq__ comparison so FA
# doesn't have to type-check e.g. an int-vs-string comparison against
# the subject's full static union (see issues/023).
def describe(val):
    match val:
        case 5:
            print("five")
        case "hi":
            print("hi-str")
        case 3.5:
            print("threehalf")
        case True:
            print("true")
        case False:
            print("false")
        case n:
            print("other:", n)

describe(5)
describe("hi")
describe(3.5)
describe(True)
describe(False)
describe([1, 2])
describe(7)
