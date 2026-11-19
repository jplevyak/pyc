# pyc check driver (added, not an upstream file: see ../PYC_CHANGES.md).
# mandelbrot2 writes its image to a BMP file and prints only a progress
# line ending in an elapsed time. This does what its __main__ does and
# prints mandel()'s result and a hash of the image. (mandel_file's own
# progress line still ends in a time; the sweep drops a line CPython varies
# on, and compares the rest.)
# corpus_sweep.sh runs this in place of mandelbrot2.py in run/check mode.
import mandelbrot2

print("mandel(1.0, 1.0, 128) =", mandelbrot2.mandel(1.0, 1.0, 128))
fname = mandelbrot2.mandel_file(max_iterations=256)
data = open(fname, "rb").read()
h = 0
for v in data:
    h = (h * 31 + v) & 0xffffffff
print("image", fname, len(data), "bytes, hash", h)
