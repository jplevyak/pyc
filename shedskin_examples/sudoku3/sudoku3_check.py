# pyc check driver (added, not an upstream file: see ../PYC_CHANGES.md).
# sudoku3's main() prints its solved grid only when x == 1999, in a loop of
# 10, so all it prints is TIME. This solves the same puzzle once and prints
# it.
# corpus_sweep.sh runs this in place of sudoku3.py in run/check mode.
import sudoku3

t = sudoku3.soduko(["800000600",
                    "040500100",
                    "070090000",
                    "030020007",
                    "600008004",
                    "500000090",
                    "000030020",
                    "001006050",
                    "004000003"])
t.check()
t.one_level_supposition()
t.check()
print(t)
