# ifa/139: a runtime-length list must not join a fixed-arity list's
# CreationSet. `[' '] * n` is built in `list.__mul__` with no static arity;
# the guard in `creation_point` once fired only when the INCOMING arity was
# known, so this str list joined the arity-3 int literals' CreationSet and
# `all`'s loop variable read {int64, str}. Fails with PYC_ARITYSTRICT=0
# (the pre-139 guard) with exactly that error; reduced from tests/builtins.py.
print(all([1, 2, 3]))
print(all([1, 0, 3]))
buffer = [' '] * len("foobar")
print(buffer)
