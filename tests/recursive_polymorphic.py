# Recursion resolved to monomorphic contours (pyc issues/025 R1
# item 5 follow-up): FA's EntrySet splitter groups RECURSIVE call
# edges by type when the recursion is level-descending -- each
# recursion depth gets its own contour, and the recursive call binds
# (via record_backedges / check_split's pending-backedge map) to the
# same ES as its split-off caller contour on the next pass. Before
# the fix (fa.cc decide_entry_set_split excluded every recursive
# edge, then short-circuited on non_rec_edges==1), a self-recursive
# function with one caller could never split: its formal carried the
# union of ALL depths' types and boxed to void* (this file's flatten
# failed to compile with "no known conversion from int64 to
# _CG_any").
#
# The separability gate (identical-or-disjoint actuals at the
# confluence position) keeps same-shape recursion over one union
# fused -- tests/expr_evaluator.py covers that side.

# Level-descending recursion: list-of-list -> list -> int64.
def flatten_sum(x):
    if isinstance(x, list):
        t = 0
        for i in range(len(x)):
            t += flatten_sum(x[i])
        return t
    return x

print(flatten_sum([[1, 2], [3, 4]]))
print(flatten_sum([10, 20, 30]))

# Same, via `for x in obj` iteration: needs per-creating-contour
# __list_iter__ CSs on top of the ES split (ifa/issues/043 shape C;
# the __pyc_clone_constants__ marking on __list_iter__.__init__) --
# with a shared iterator CS, `thelist` unions every level's lists
# and the element var re-fuses the freshly separated contours.
def deep_copy_list(x):
    if isinstance(x, list):
        r = []
        for e in x:
            r.append(deep_copy_list(e))
        return r
    return x

orig = [[1, 2], [3, 4]]
cpy = deep_copy_list(orig)
cpy[0][0] = 99
print(orig)
print(cpy)

# Monomorphic recursion: the recursive call binds to the same
# contour as the top-level caller's.
def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)

print(fib(20))

# Mutual recursion with descending types across the pair.
def g(x):
    return h(x)

def h(x):
    if isinstance(x, list):
        r = []
        for i in range(len(x)):
            r.append(g(x[i]))
        return r
    return x

print(g([5, 6, 7]))
