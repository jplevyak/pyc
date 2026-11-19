# pyc check driver (added, not an upstream file: see ../PYC_CHANGES.md).
# minpng writes a PNG and prints only TIME. Its code runs at module level,
# so importing it runs the whole program (TIME is the one line CPython
# varies on, and the sweep drops it); this then prints a hash of the PNG,
# which exercises its zlib stream, CRC and Adler-32 code.
# corpus_sweep.sh runs this in place of minpng.py in run/check mode.
import minpng

data = open("minpng.png", "rb").read()
h = 0
for v in data:
    h = (h * 31 + v) & 0xffffffff
print("minpng.png", len(data), "bytes, hash", h)
print("first", list(data[:16]))
