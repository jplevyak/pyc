# bytes() of a deque or a range of ints (rsync's `bytes(window)`), and a
# file's `closed` attribute (rsync's `if datastream.closed`).
import collections

window = collections.deque(b"abc")
window.append(ord("d"))
print(bytes(window), bytes(range(65, 70)), bytes(collections.deque()))
f = open("bytes_from_iterables.bin", "wb")
print(f.closed)
f.write(bytes(window))
f.close()
print(f.closed)
g = open("bytes_from_iterables.bin", "rb")
print(g.read(), g.closed)
g.close()
