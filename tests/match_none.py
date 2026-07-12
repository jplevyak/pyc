# case None: combined with anything beyond wildcard/other-None arms
# hits a known pyc code-generation limitation (see issues/023) --
# build_match_pyda refuses that combination at compile time. The one
# safe combination, covered here: None with a wildcard fallback.
def describe(val):
    match val:
        case None:
            print("none")
        case _:
            print("other")

describe(None)
describe(5)
describe("hi")
describe([1, 2])
