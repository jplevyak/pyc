# ifa/210 (closed 2026-11-19): a list display whose elements are function
# constants. The C backend stored the bare function into the element's
# `void *` member (a C++ error); the LLVM backend stored no elements, so every
# call dispatched on null. Both now store the function's address.
fs = [lambda x: x - 7, lambda x: x * x]
for k in fs:
    print(k(5))
