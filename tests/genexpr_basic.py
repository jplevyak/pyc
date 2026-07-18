# issues/008, issues/014, issues/025 (genexpr bucket, 2026-07-18):
# generator expressions used to crash the compiler (issue 008's
# original bug), then were rejected with a clean fail() as an interim
# fix. Real lazy-generator semantics (backed by issue 014's yield
# machinery) remain unimplemented, but every genexpr in the shedskin
# corpus is consumed immediately by a single eager builtin over a
# finite iterable -- so genexpr now materializes eagerly into a list
# instead, exactly like a list comprehension (and like `{... for
# ...}` already does). Covers the consumer shapes actually found in
# the corpus: bare iteration, sum/list/dict/join/sorted/any/all/min.
g = (x * 2 for x in [1, 2, 3])
for v in g:
    print(v)

print(sum(x * x for x in range(5)))
print(list(x for x in range(4)))
print(dict((x, x * x) for x in range(3)))
print(', '.join(str(x) for x in [1, 2, 3]))
print(sorted((x, -x) for x in [3, 1, 2]))
print(any(x > 2 for x in [1, 2, 3]))
print(all(x > 0 for x in [1, 2, 3]))
print(min(x for x in [5, 2, 8, 1]))

# nested/filtered form, matching sat.py's `all(info != NONE for info
# in self.assigns)` shape and sokoban.py's `max(len(r) for r in
# data)`.
print(max(len(row) for row in [[1], [1, 2, 3], [1, 2]]))
