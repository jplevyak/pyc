def classify(val):
    match val:
        case [[a, b], c]:
            print("nested:", a, b, c)
        case (1, x):
            print("tuple-lit-first:", x)
        case [1 | 2, y]:
            print("or-first:", y)
        case [a, b] if a > b:
            print("descending pair:", a, b)
        case [a, b]:
            print("pair:", a, b)
        case [a, b, c]:
            print("triple:", a, b, c)
        case []:
            print("empty")
        case other:
            print("other:", other)

classify([[1, 2], 3])
classify((1, 99))
classify([2, 50])
classify([5, 3])
classify([3, 5])
classify([7, 8, 9])
classify([])
classify(42)
classify("hi")
