# issues/040: "%d" % <float> (Python truncates; valid and common)
# produced deterministic garbage instead of the truncated integer --
# a double reaching a C printf %d specifier is undefined behavior
# (wrong varargs register class on x86-64 SysV). Same class of bug
# the other direction (%f/%e/%g given an int).
x = 3.7
print("%d" % x)
print("%i" % 4.2)
print("%u" % 5.9)
print("%d" % -3.9)

y = 7
print("%f" % y)
print("%e" % 3)
print("%g" % 2)

print("%d %d %d" % (1.5, 2.5, 3.5))
print("%s and %d" % ("hi", 4.9))

# Already-matching specs must stay unaffected.
print("%d" % 7)
print("%f" % 3.7)

# The exact color.__str__ shape from yopyra (issues/025) that
# motivated this issue.
class color:
    def __init__(self, r, g, b):
        self.r = r
        self.g = g
        self.b = b
    def __str__(self):
        return "%d %d %d" % (max(0.0, min(self.r*255.0, 255.0)),
                             max(0.0, min(self.g*255.0, 255.0)),
                             max(0.0, min(self.b*255.0, 255.0)))

print(str(color(0.15, 0.5, 0.99)))
