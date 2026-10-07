# A text file's `.buffer` is the binary stream under it, as in CPython
# (tarsalzp reads and writes `sys.stdin.buffer` / `sys.stdout.buffer`).
import sys

f = open("file_buffer.txt", "w")
f.buffer.write(b"bytes line\n")
f.close()
g = open("file_buffer.txt")
print(repr(g.buffer.read()))
g.close()
sys.stdout.flush()
sys.stdout.buffer.write(b"to stdout.buffer\n")
sys.stdout.buffer.flush()
