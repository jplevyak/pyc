# A circular import: circ_pkg imports circ_pkg.sub, which imports circ_pkg.
# Each module's names are read at call time, after both have run. The
# compiler used to recurse until its stack overflowed.
import circ_pkg
import circ_pkg.sub
from circ_pkg.sub import up
print(circ_pkg.twice(), circ_pkg.sub.up(), up(), circ_pkg.base())
