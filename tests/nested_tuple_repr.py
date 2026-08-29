# issues/119: printing a NESTED tuple aborts at runtime.
#
#     print((1, (2, 3)))
#
# compiles with ZERO diagnostics, exits 0 from the compiler, and then:
#
#     _CG_string _CG_f_176_14(_CG_any): Assertion
#       `!"runtime error: matching function not found"' failed.
#
# The failing function returns _CG_string and takes _CG_any -- the
# __repr__ dispatch on a tuple element whose type is the ANY type,
# because this tuple's elements are an int64 and another tuple. A tuple
# stores its elements as record fields, and tuple.__str__'s loop
# dispatches __repr__ per element with no single resolution.
#
# `print((1, 2))` and `print([(1, 2)])` are both fine -- it is
# specifically a tuple element sitting alongside a scalar one.
#
# NOT ifa/061, which is a LIST of tuples contaminated by an unrelated
# list's .sort() clone and fails at C COMPILE time. They were found
# together only because 061's repro prints a list of nested tuples.
print((1, (2, 3)))
