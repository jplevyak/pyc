# issues/113: `import a.b` binds only the TOP name, with the rest of the
# chain reached as ATTRIBUTES of the package -- CPython's rule.
#
# Before packages resolved, `a.b` never matched a file and the resolver
# fell back to binding `a`, which happened to look right. Once `a.b`
# DID resolve, binding it produced a symbol literally named "a.b", so
# `a` was unbound and `a.b.c()` failed with "'a' has no type".
#
# `import os.path` exercises the OTHER path that must keep working: no
# os/path.py exists, so resolution falls back to the top component and
# `path` is an ordinary attribute of the os shim.
import pyc_pkg.sub
import pyc_pkg.deep.leaf
import pyc_pkg.sub as aliased
import os.path

print(pyc_pkg.sub.subfn(1))
print(pyc_pkg.deep.leaf.leaffn())
print(aliased.subfn(2))
print(os.path.join("a", "b"))
