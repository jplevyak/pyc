def sig(x, shift=0, comp=1):
    return 1 / (1 + comp * x - shift)

print(sig(2.0) > 0.0)
a = 15 * sig(0.5, 0.5, 10)
print(a > 0.0)
