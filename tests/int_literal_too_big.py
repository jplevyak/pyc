# issues/177: a literal that does not fit 64 bits has no value to wrap to,
# so it is refused in every mode. strtoll used to saturate it to INT64_MAX
# silently.
print(99999999999999999999)
