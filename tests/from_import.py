# Regression: `from X import Y [as Z]` binds a module's function,
# class, and data into the importing scope so they are usable and
# callable across modules (issue 025 bucket C, module subsystem).
from from_import_helper import add, triple as tri, Point

def main():
    print(add(2, 3))
    print(tri(4))
    p = Point(7)
    print(p.x)
    print(p.doubled())

main()
