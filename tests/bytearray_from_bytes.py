# bytearray(x) for bytes, bytearray and a list of ints, and bytes(bytearray)
# (sokoban's move/push). The raw @vector constructor takes only a length;
# bytearray(x) now dispatches to x.__pyc_tobytearray__(). Also
# filter(None, it), which keeps the true items (sokoban's board parse).
data = b'ab cd'
d2 = bytearray(data)
d2[2] = ord('X')
print(bytes(d2), len(d2), d2[0])
print(bytearray(3), bytes(bytearray(2)))
print(bytearray([65, 66]), bytes(bytearray(b'hi')), bytearray(bytearray(b'q')))
print(list(filter(None, ["a", "", "b", ""])), list(filter(None, [0, 1, 2, 0])))
