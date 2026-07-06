# Regression: tuple-unpacking assignment to non-name targets --
# attributes (self.x), subscripts (a[i]), and nested tuples -- not
# just plain names. The classic swap `a, b = b, a` and its attribute
# and subscript forms must work (issue 025 illegal-destructuring).
class Vec:
    def __init__(self, x, y):
        self.x = x
        self.y = y

    def swap(self):
        self.x, self.y = self.y, self.x

def main():
    v = Vec(1, 2)
    v.swap()
    print(v.x)                       # 2
    print(v.y)                       # 1

    a = [10, 20, 30]
    a[0], a[2] = a[2], a[0]          # subscript swap
    print(a[0])                      # 30
    print(a[2])                      # 10

    p = Vec(0, 0)
    m, p.x, (n, p.y) = 5, 6, (7, 8)  # mixed name / attribute / nested
    print(m)                         # 5
    print(p.x)                       # 6
    print(n)                         # 7
    print(p.y)                       # 8

    b, c = c0, c1 = 100, 200         # chained + plain names still work
    print(b + c + c0 + c1)           # 600

main()
