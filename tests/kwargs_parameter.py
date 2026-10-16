# issues/182: a **kwargs parameter collects the unmatched keyword arguments.
def f(**kw):
    return kw["x"] + len(kw)

print(f(x=3, y=4))
