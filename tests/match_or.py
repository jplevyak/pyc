def classify(val):
    match val:
        case 1 | 2 | 3:
            print("small")
        case 10 | 20:
            print("medium")
        case x:
            print("other:", x)

for v in [1, 2, 3, 10, 20, 99]:
    classify(v)
