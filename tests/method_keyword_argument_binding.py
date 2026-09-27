# A keyword argument to a METHOD bound two slots late: partial_application
# placed the call's names after `def->rvals.n` (4, the `obj.f` period send)
# instead of after the closure's captured values (2, [fun, self]), and the
# match resolved it to the previous formal. `s.f(1, reason_txt="x")` set
# `reason` and left `reason_txt` None, silently (shedskin_examples/sat).
class C:
    def name(self):
        return "c"
class S:
    def f(self, lit, reason=None, reason_txt=None):
        self.reason = reason
        self.txt = reason_txt
        return lit
    def g(self, a, b=1, c=2, d=3):
        return a * 1000 + b * 100 + c * 10 + d
s = S()
s.f(1, reason_txt="learnt")
print(s.reason, s.txt)
s.f(2, C())
print(s.reason.name(), s.txt)
print(s.g(5, d=9), s.g(5, c=7), s.g(5, 6, d=0), s.g(a=4, b=3, c=2, d=1))
