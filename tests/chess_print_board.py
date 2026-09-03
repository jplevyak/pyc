# shedskin_examples/chess's printBoard, in isolation.
#
# chess.py had this commented out and never called, and it had been
# mis-translated from Python 2 -- the original `print pieces[...],`
# lost its trailing comma, so uncommenting it would have printed one
# character per LINE rather than a rank per line. Restored 2026-09-03.
#
# chess itself cannot compile (ifa/issues/118: a {bool, None} field
# mixes 1- and 8-byte members), so this pins the printing separately.
# What it exercises: 0x88 board indexing, ' '.join over a list of str,
# and NEGATIVE string indexing -- black pieces are stored negative and
# index `pieces` from the end, so pieces[-1] == 'P'.
pieces = ".pnbrqkKQRBNP"

setup = ((4, 2, 3, 5, 6, 3, 2, 4) + (0,) * 8 +
         (1,) * 8 + (0,) * 8 +
         ((0,) * 8 + (0,) * 8) * 4 +
         (-1,) * 8 + (0,) * 8 +
         (-4, -2, -3, -5, -6, -3, -2, -4) + (0,) * 8)


def printBoard(board):
    for i in range(7, -1, -1):
        row = []
        for j in range(8):
            ix = i * 16 + j
            row.append(pieces[board[ix]])
        print(' '.join(row))
    print()


printBoard(list(setup))
