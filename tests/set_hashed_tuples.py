# issues/118: a set of tuples hashes by content.
tt = set([(1, 2), (3, 4), (1, 2)])
print(len(tt), (3, 4) in tt, (4, 3) in tt)
tt.add((4, 3))
print(len(tt), (4, 3) in tt)
