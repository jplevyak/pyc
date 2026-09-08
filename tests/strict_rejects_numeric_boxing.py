# ifa/issues/145: --strict must ERROR on anything that would require
# boxing, and an {int64, float64} member mix is such a case.
#
# Author's directive, 2026-09-08: "pyc has a strict and permissive mode,
# and any automatic coercion should be permissive only", and "strict mode
# should error on anything which would require boxing."
#
# `coerce_annotate` widens a pure-numeric confluence to the wider type.
# That is automatic AND OBSERVABLE -- permissive pyc prints `1.0` here
# where CPython prints `1` -- so it is a permissive-Python fallback, and
# --strict promises none of those. It used to run in strict mode anyway,
# because it asked no mode question at all.
#
# With the coercion declined, the mix is left standing and the BOXING
# violation on cs->vars catches it; `fruntime_errors` false makes a type
# violation a hard compile error. This test pins that.
#
# The permissive side is covered by tests/mixed_numeric_field.py and the
# corpus, which compile in the default mode.
import sys

class V:
    def __init__(self):
        self.d = 0.0

    def set(self, v):
        self.d = v

a = V()
a.set(1.5)
b = V()
b.set(len(sys.argv))      # a runtime int into a float member
print(a.d, b.d)
