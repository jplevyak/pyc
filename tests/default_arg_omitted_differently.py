# issues/101: two call sites of the same function that omit DIFFERENT
# defaulted parameters. The second call must reset `reason` to its
# default (None); pyc keeps the first call's Clause, so this prints 7
# where CPython prints 0 -- a silent wrong answer, not a diagnostic.
#
# Narrowed: not __slots__, not None-as-the-default, and not keyword
# syntax per se. Passing both arguments explicitly, or always omitting
# the SAME parameter, is clean.
#
# The check files describe the CORRECT behaviour; the .known_issue tag is
# what keeps this from failing the suite. Delete the tag when fixed.
class Clause:
    def why(self):
        return 7

class VarInfo:
    __slots__ = ['reason', 'reason_txt']
    def __init__(self):
        self.reason = None
        self.reason_txt = None

class Solver:
    def __init__(self):
        self.v = VarInfo()

    # two defaulted keyword params, each stored into its OWN field
    def enqueue(self, reason=None, reason_txt=None):
        self.v.reason = reason
        self.v.reason_txt = reason_txt

    def run(self):
        self.enqueue(reason=Clause())      # only the Clause param
        self.enqueue(reason_txt="learnt")  # only the str param
        c = self.v.reason
        if c:
            return c.why()                 # legal only if reason stayed Clause
        return 0

s = Solver()
print(s.run())
