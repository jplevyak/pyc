# pyc check driver (added, not an upstream file: see ../PYC_CHANGES.md).
# circle.py is the packing module behind circle_main.py, a pygame program;
# its own __main__ only touches each function once and prints nothing but
# sys.version. This packs a fixed set of 40 circles for 50 rounds, as
# circle_main.py's loop does, and prints where they end up.
# corpus_sweep.sh runs this in place of circle.py in run/check mode.
import circle

circle.setup(400, 300)
circles = []
for i in range(40):
    x = 200.0 + ((i * 37) % 101) - 50
    y = 150.0 + ((i * 53) % 89) - 44
    circles.append(circle.Circle(x, y, 4.0 + (i * 7) % 13, (i % 256,)))
for step in range(50):
    circle.pack(circles, 0.1, 2, circles[0])
for c in circles:
    print("%d %.6f %.6f %.1f" % (c.color[0], c.x, c.y, c.radius))
print(sum(1 for a in circles for b in circles if a is not b and a.intersects(b)))
