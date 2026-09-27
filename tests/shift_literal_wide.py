# The C backend emitted `7 << 40` as `(7) << (40)`: a 32-bit int shift,
# undefined past bit 31 (it printed -3, and on another run 104249815179696).
print(7 << 40, 0x80 << 24, 200 << 24, (1 << 63) - 1, 1 << 62)
x = 3
print(x << 40, (x << 33) >> 33)
