def check(v):
    if isinstance(v, list):
        return 1
    return 0

lst = [[1, 2], "hello"]
for v in lst:
    print(check(v))
