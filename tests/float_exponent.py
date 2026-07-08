# Exponent float literals with explicit '+' sign: the grammar's NUMBER
# regex had a double-escaped plus ((\\+|-) in python.g), so `1e+00`
# failed to scan while `1e-00` worked. Comparisons (not printing)
# avoid the unrelated float-repr precision divergence.
def main():
    print(4.84e+00 == 4.84)
    print(1.0E+5 == 100000.0)
    print(2.5e-10 > 0.0)
    print(2e10 == 20000000000.0)
    print(1.66007664274403694e-03 < 1.0)
main()
