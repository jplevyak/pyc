# issues/113: package imports. Exercises, in order:
#   - `from PKG import SUBMODULE`      (pyc_pkg/sub.py, bound as a module)
#   - `from PKG.SUB import name`       (dotted path resolution)
#   - `from PKG.A.B import name`       (multi-level dotted path)
#   - `from .helper import hval`       (relative, inside pyc_pkg/sub.py)
#   - `from ..helper import hval`      (parent-relative, inside deep/leaf.py)
#   - an EMPTY __init__.py             (both package dirs), which the
#                                       parser used to reject outright
from pyc_pkg import sub
from pyc_pkg.sub import subfn
from pyc_pkg.deep.leaf import leaffn

print(sub.subfn(1))
print(subfn(2))
print(leaffn())
