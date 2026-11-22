# isinstance(x, str) on a value that is None or a str. A str carries no
# class tag, so no tag compare can answer, but None is the null pointer:
# the test is `x is not None`. str.__eq__/__ne__ ask exactly this of their
# operand, so comparing a str with a maybe-None str reached it
# (msp_ss's `bauratetable[self.cpu]`, once pyc_lib/serial made it live).
for v in [None, "abc", "zz"]:
    print(isinstance(v, str), "abc" == v, "abc" != v)
