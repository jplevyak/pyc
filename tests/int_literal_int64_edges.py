# issues/177: integer literals at the edges of int64 that DO fit, in every
# base. -9223372036854775808 is a folded unary minus on 2^63, which fits
# only negated; the C backend used to spell it as that same expression,
# which C reads as unsigned and clang warns about.
print(9223372036854775807)
print(-9223372036854775808)
print(-9223372036854775807 - 1 == -9223372036854775808)
print(0x7FFFFFFFFFFFFFFF, -0x8000000000000000)
print(0o777777777777777777777, -0o1000000000000000000000)
print(0b0111111111111111111111111111111111111111111111111111111111111111)
print(9_223_372_036_854_775_807)
m = -9223372036854775808
print(m + 1, m < 0, (m + 1).bit_count())
