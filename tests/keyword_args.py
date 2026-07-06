# Regression: keyword arguments (name=value) in calls. Previously
# keyword args were parsed but dropped from the emitted call, leaving
# the callee's params untyped -> spurious "unresolved call" downstream
# (issue 025 bucket A). Covers plain functions, methods, and class
# constructors (whose __new__ wrapper formals now carry the __init__
# parameter names so keyword actuals can bind). NOTE: keyword args must
# be given in declaration order today; out-of-order still fails (safely).
class Box:
    def __init__(self, low, high):
        self.low = low
        self.high = high

    def span(self, lo, hi):
        return hi - lo

def combine(a, b, c):
    return a * 100 + b * 10 + c

def main():
    b = Box(low=2, high=9)               # constructor, all keyword
    print(b.high - b.low)
    print(b.span(lo=1, hi=4))            # method, all keyword
    print(combine(1, b=2, c=3))          # function, positional + keyword
    print(combine(1, 2, c=3))            # function, one keyword

main()
