def classify(val):
    match val:
        case 1:
            print("one")
        case 2:
            print("two")
        case x:
            print("other:", x)

classify(1)
classify(2)
classify(42)
classify(-7)
