# ifa/156: tictactoe's shape. Two grids of rows, each row made by the same
# `n * [0]` (one creation point, inside `list.__mul__`); one grid only
# ever holds ints, the other gets float scores over its initial zeros.
# Both outer lists are filled through one shared `append`. CPython prints
# the board's `2` and the scores' `0.5`; pyc must split the row contour by
# caller rather than widen the board to `2.0`. The one warning left (.check) is the scores rows,
# which really do hold the int 0 and then floats.
class Game:
    def __init__(self, n):
        self.board = [n * [0] for _ in range(n)]

    def scores(self, n):
        s = [n * [0] for _ in range(n)]
        for r in range(n):
            for c in range(n):
                s[r][c] = (r + c) / 4.0
        return s


g = Game(3)
g.board[1][2] = 2
sc = g.scores(3)
print(g.board[1][2], g.board[0][0])
print(sc[1][1], sc[2][2])
