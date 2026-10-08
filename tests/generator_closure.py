# A nested generator that captures variables of its enclosing function
# (sunfish's `moves()` inside `bound`) had no generator wrapper: calling it
# returned the body's raw result, typed as its yields, so `for x in
# moves()` iterated the FIRST YIELDED VALUE.
def outer(n):
    def moves():
        yield None, n
        for m in range(2):
            yield (m, m + 1), n + m
    best = -1
    for move, score in moves():
        if score > best:
            best = score
    return best

def outer2(n):
    def gen():
        yield n
        yield n + 1
    return [x for x in gen()]

class Searcher:
    def __init__(self):
        self.k = 10
    def run(self):
        def inner(lo):
            for i in range(lo, lo + 3):
                yield i * self.k
        return list(inner(2))

print(outer(5), outer2(5), Searcher().run())
