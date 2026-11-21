# issues/202: Path.open() in a binary mode is refused when called, before
# anything is read: its result would be a binary file at one call and a text
# file at another, and pyc chooses between the two from a literal mode at
# the call to open(). read_bytes(), write_bytes() and open(path, 'rb') work.
from pathlib import Path

p = Path("_pathlib_open_binary_refused.bin")
p.write_bytes(b"ab")
print(p.read_bytes())
f = p.open("rb")
print(f.read())
