# A file's write() returns the count written, as CPython's does: characters
# for a text file, bytes for a binary one. It returned None.
f = open("_file_write_count.txt", "w")
print(f.write("hello"), f.write(""), f.write("a\nb\n"))
f.close()
g = open("_file_write_count.bin", "wb")
print(g.write(b"ab\x00c"))
g.close()
h = open("_file_write_count.txt")
print(repr(h.read()))
h.close()
import os
os.remove("_file_write_count.txt")
os.remove("_file_write_count.bin")
