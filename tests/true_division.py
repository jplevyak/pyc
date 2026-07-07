# Python 3 division semantics: `/` on ints is TRUE division (float);
# `//` is floor division (int). pyc's int.__truediv__ previously did
# C integer division (3/2 == 1), which broke mandelbrot's
# `y/40 - 0.5` coordinates (issue 025 first green example).
def main():
    print(3 / 2)          # 1.5
    print(3 // 2)         # 1
    print(8 / 4)          # 2.0 (still float!)
    print(7 / 2.0)        # 3.5
    print(1 / 8)          # 0.125
    y = -20
    print(y / 40 - 0.5)   # -1.0
    x = 7
    x /= 2
    print(x)              # 3.5

main()
