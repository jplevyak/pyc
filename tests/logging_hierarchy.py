# issues/197: the hierarchy built out of order, a FileHandler with a name
# Filter, asctime's shape, and what CPython raises.
import logging
import os

lg = logging.getLogger("svc")

# A child made before its parent is re-parented to it.
abc = logging.getLogger("a.b.c")
print(abc.parent.name)
a = logging.getLogger("a")
print(abc.parent.name, a.parent.name)
ab = logging.getLogger("a.b")
print(abc.parent.name, ab.parent.name)
print(logging.getLogger("a.b") is ab, ab.getChild("c") is abc)

# A name filter on a handler.
fh_path = "logging_hierarchy_log.txt"
fh = logging.FileHandler(fh_path, "w")
fh.setFormatter(logging.Formatter("%(name)s:%(message)s"))
fh.addFilter(logging.Filter("a.b"))
a.addHandler(fh)
abc.warning("from abc")
a.warning("from a")
logging.getLogger("a.bx").warning("from a.bx")
fh.close()
f = open(fh_path)
print(f.read(), end="")
f.close()
os.remove(fh_path)

# asctime: its shape (the value is the time).
rec = logging.LogRecord("t", logging.INFO, "p", 1, "m %d", (5,), None)
print(rec.getMessage(), rec.levelname, len(logging.Formatter().formatTime(rec)))
print(logging.Formatter("%(asctime)s", "[T]").format(rec))
print(logging.Formatter("%(message)s %(msecs)03.0f").format(rec)[0:4])

# What CPython raises.
try:
    lg.setLevel("BOGUS")
except ValueError as e:
    print("ValueError", e)
try:
    logging.Formatter("no fields here")
except ValueError as e:
    print("ValueError", e)
try:
    lg.log("x", "y")
except TypeError as e:
    print("TypeError", e)
