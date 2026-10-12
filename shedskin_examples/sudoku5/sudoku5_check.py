# pyc check driver (added, not an upstream file: see ../PYC_CHANGES.md).
# sudoku5 prints its solutions only when n == 9999, in a loop of 100, so all
# it prints is TIME. This solves the same grid once and prints every
# solution.
# corpus_sweep.sh runs this in place of sudoku5.py in run/check mode.
import sudoku5

grid = [
    [5, 3, 0, 0, 7, 0, 0, 0, 0],
    [6, 0, 0, 1, 9, 5, 0, 0, 0],
    [0, 9, 8, 0, 0, 0, 0, 6, 0],
    [8, 0, 0, 0, 6, 0, 0, 0, 3],
    [4, 0, 0, 8, 0, 3, 0, 0, 1],
    [7, 0, 0, 0, 2, 0, 0, 0, 6],
    [0, 6, 0, 0, 0, 0, 2, 8, 0],
    [0, 0, 0, 4, 1, 9, 0, 0, 5],
    [0, 0, 0, 0, 8, 0, 0, 7, 9],
]
count = 0
for solution in sudoku5.solve_sudoku((3, 3), grid):
    count += 1
    print("\n".join(str(s) for s in solution))
print("solutions", count)
