# More list ordering: <, >, <=, >= incl. empty lists, min/max over
# (score, list) tuples with ties, sorted() of lists (issues/122).
print([1, 2] < [1, 3], [2] > [1, 5], [1, 2] <= [1, 2], [1] >= [1, 0], [] < [0], [3] < [])
print(max([(0.5, [1, 2]), (0.5, [1, 3])]), min([(1, [9]), (1, [2, 2])]))
print(sorted([[3, 1], [1, 2], [1], [2, 0]]), ["b", "a"] < ["b", "b"])
