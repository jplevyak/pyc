# pyc check driver (added, not an upstream file: see ../PYC_CHANGES.md).
# othello2 prints "N nodes in T seconds (R/second)": the node count is its
# result, the rest is timing. This runs the same search and prints the node
# count and the move it chose.
# corpus_sweep.sh runs this in place of othello2.py in run/check mode.
import othello2

othello2.NODES = 0
board = othello2.empty_board()
state = othello2.parse_state(board)
move = othello2.minimax_ab(state, othello2.BLACK, 0, 10, True)
print("nodes", othello2.NODES, "move", move)
