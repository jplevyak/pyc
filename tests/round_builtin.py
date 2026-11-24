# round() as CPython's (issues/211): half to even, ndigits on floats (the
# exact binary value, so round(2.675, 2) is 2.67) and on ints, -0.0, the
# NaN and infinity errors, __round__ on a user class.
vals = [0.5, 1.5, 2.5, -0.5, -1.5, -2.5, 2.675, 0.125, 0.375, 1.0, -0.0, 3.14159, 1e16 + 1, -7.5]
print([round(v) for v in vals])
print([round(v, 2) for v in vals])
print([round(v, 0) for v in vals])
print([round(v, 1) for v in vals])
print(round(2.675, 2), round(1.005, 2), round(0.285, 2), round(1234.5678, -2), round(1250.0, -2), round(1350.0, -2), round(-1250.0, -2))
print(round(55.0, -2), round(50.0, -2), round(51.0, -2), round(-51.0, -2), round(4.0, -1), round(5.0, -1), round(15.0, -1), round(0.001, -3))
print(round(-0.001, 2), round(-0.4, 0), round(123.456, 10), round(1e-320, 400), round(1.5, -400), round(9.99999, 3), round(999.96, 1))
print(round(1e300, -298), round(1.23456789e-5, 7), round(2.0 ** 60, -1), round(5e-324, 324))
print([round(n) for n in [0, 7, -7]], [round(n, -1) for n in [15, 25, -15, -25, 14, 16]], round(1234567, -3), round(1500, -3), round(2500, -3), round(-2500, -3))
print(round(123, 2), round(9, -20), round(-9223372036854775807, -18))
print(int(round(3.5)), int(round(4.5)), round(2.5) + 1)

class Money:
    def __init__(self, c):
        self.c = c
    def __round__(self, n=0):
        return Money(round(self.c, n))
print(round(Money(2.567), 1).c, round(Money(3.5)).c)

for bad in [float('nan'), float('inf'), float('-inf')]:
    try:
        print(round(bad))
    except ValueError as e:
        print("ValueError", e)
    except OverflowError as e:
        print("OverflowError", e)
print(round(float('nan'), 2), round(float('inf'), 2))
