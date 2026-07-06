# Regression: an attribute name in `x.attr` must NOT be resolved as a
# variable reference by the symbol-table pass. It previously created a
# spurious module global for every attribute name, which then collided
# with any same-named reassigned parameter/local, producing a bogus
# "'X' redefined as local" (issue 025 bucket B; go.py `color`). Here
# `color` is both an attribute (render) and a reassigned param (update).
class Sq:
    def __init__(self, color):
        self.color = color

def render(squares):
    return [s.color for s in squares]

def update(color):
    for k in range(2):
        if color == 1:
            color = 2
        else:
            color = 1
    return color

def main():
    sqs = [Sq(1), Sq(2)]
    print(render(sqs))
    print(update(1))
    print(update(2))

main()
