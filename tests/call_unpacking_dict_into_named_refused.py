# issues/189: `**d` binds only a `**kw` parameter. Into named parameters it
# needs the keys in the type, which a dict does not have.
def f(a, b):
    return a - b

print(f(**{"a": 1, "b": 2}))
