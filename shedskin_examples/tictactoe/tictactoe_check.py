# pyc check driver (added, not an upstream file: see ../PYC_CHANGES.md).
# tictactoe's aigame(n) prints the final board only when n == 999, and its
# __main__ only plays n < 40, so all it prints is TIME. aigame(999) plays
# the same deterministic game and prints the board and the result.
# corpus_sweep.sh runs this in place of tictactoe.py in run/check mode.
import tictactoe

tictactoe.aigame(999)
