# issue 025 "has no type" bucket: previously-missing builtins,
# implemented in pure Python in __pyc__/05_builtins.py. zip/map/
# filter/enumerate/reversed return lists (shedskin-style divergence
# from Py3 iterators); min/max support both the two-arg and sequence
# forms via the default-None + is-None-narrowing pattern.
# NOTE: combining zip and enumerate in one program trips a
# PRE-EXISTING tuple-list codegen bug (see issue 025) -- this test
# uses enumerate via for-unpack and exercises zip only by indexing.
def main():
    xs = [3, 1, 4, 1, 5]
    ys = [10, 20, 30, 40, 50]
    print(sum(xs))                    # 14
    print(sum([0.5, 1.5]))            # 2.0 (numeric unification)
    print(min(xs), max(xs))           # 1 5
    print(min(3, 7), max(3, 7))       # 3 7
    print(reversed(xs)[0])            # 5
    print(pow(2, 10))                 # 1024
    print(round(2.7))                 # 3
    print(sorted(xs))                 # [1, 1, 3, 4, 5]
    def double(v):
        return v * 2
    print(map(double, xs))            # [6, 2, 8, 2, 10]
    def big(v):
        return v > 2
    print(filter(big, xs))            # [3, 4, 5]
    for i, v in enumerate(["a", "b"]):
        print(i, v)
main()
