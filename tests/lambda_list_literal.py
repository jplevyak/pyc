# ifa/210: a list display whose elements are function constants (two
# lambdas, or a lambda beside a def). The C backend stores the bare
# function into the element's `void *` member without a cast, which C++
# rejects; the LLVM backend allocates the list and stores no elements, so
# every call dispatches on null (it printed 0 0, and traps since ifa/200
# A3). Two `def`s work: they reach the list through their global slots.
fs = [lambda x: x - 7, lambda x: x * x]
for k in fs:
    print(k(5))
