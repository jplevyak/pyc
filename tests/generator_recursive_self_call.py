# A recursive generator's call to ITSELF must reach the generator wrapper,
# not the coroutine body (which returns a handle, an int). sudoku5's
# `for solution in solve(X, Y, solution)` typed `solution` as int|tuple.
def solve(solution, depth):
    if depth == 0:
        yield list(solution)
    else:
        solution.append((1, 2, 3))
        for sol in solve(solution, depth - 1):
            yield sol
        solution.pop()

n = 0
for s in solve([], 2):
    for (a, b, c) in s:
        n += a
print(n)
