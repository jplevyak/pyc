# zip() returns a list of pairs (see builtins_batch.py note on the
# pre-existing tuple-list codegen bug that keeps zip and enumerate in
# separate tests).
def main():
    xs = [3, 1, 4]
    ys = [10, 20, 30, 40]
    z = zip(xs, ys)
    print(len(z))       # 3 (shorter input)
    print(z[0][0], z[0][1], z[2][1])  # 3 10 30
    for a, b in z:
        print(a + b)
main()
