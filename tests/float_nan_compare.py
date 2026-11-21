# Comparisons with a NaN, as IEEE and Python have them: only != is true.
# The LLVM backend made NaN != NaN false (an ordered compare, issues/207).
n = float("nan")
print(n != n, n == n, n < 1.0, n > 1.0, n <= n, n >= n, 1.0 != 2.0, 1.0 != 1.0)
