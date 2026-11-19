# A string whose value begins and ends with a quote keeps it. The LLVM
# backend used to strip a surrounding pair of quotes from every string
# constant (for the V frontend, which kept a literal's source quotes), so
# under -b this printed `hi` and `3 q`.
print('"hi"')
x = '"q"'
print(len(x), x)
print('""')
