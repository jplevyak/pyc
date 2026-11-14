# issues/182: `*t` is the callee's whole *args. Unpacking it into named
# parameters needs the tuple's arity at the call, which pyc does not bind;
# refused rather than passed as one argument (which it silently was).
def f(a, b):
    return a - b

t = (5, 2)
print(f(*t))
