# Regression: `import X [as Z]` binds X as a compile-time module
# namespace; `X.func()`, `X.Class(...)`, and `X.DATA` resolve to the
# module's members (issue 025 bucket C, module subsystem phase 2).
import from_import_helper
import from_import_helper as helper

def main():
    print(from_import_helper.add(2, 3))
    print(helper.triple(4))
    p = from_import_helper.Point(7)
    print(p.x)
    print(p.doubled())
    print(helper.GREETING)

main()
