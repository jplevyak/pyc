# pyc check driver for minilight (added, not an upstream file: see
# ../PYC_CHANGES.md). minilight is a Monte Carlo renderer with an UNSEEDED
# random(), so the image it writes differs from run to run even under
# CPython, and its stdout is only a TIME line. This seeds the generator, runs
# one render (minilight.py runs two, the first to warm up PyPy), and prints
# the image's size, a hash of its pixels, and a few pixel values, so the
# render can be compared with CPython's. pyc's random is CPython's MT19937
# bit for bit (pyc_lib/random.py), so a seeded render must match exactly.
# corpus_sweep.sh runs this in place of minilight.py in run/check mode.
import random
from ml import entry

random.seed(1)
entry.main('cornellbox.txt')

data = open('cornellbox.txt.ppm', 'rb').read()
# minilight's header (ml/image.py): "P6", a comment, a blank line,
# "width height", "255", then the pixels.
lines = data.split(b'\n', 5)
pixels = lines[5]
h = 0
for v in pixels:
    h = (h * 31 + v) & 0xffffffff
print("header", lines[0], lines[3], lines[4])
print("pixels", len(pixels), "hash", h)
print("first", list(pixels[:12]))
mid = len(pixels) // 2
print("middle", list(pixels[mid:mid + 12]))
