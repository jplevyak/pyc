def classify(val, flag):
    match val:
        case 1 if flag:
            print("one-flagged")
        case 1 | 2 if flag:
            print("small-flagged")
        case x if x > 100:
            print("huge:", x)
        case _ if flag:
            print("other-flagged")
        case _:
            print("plain other")

classify(1, True)
classify(1, False)
classify(2, True)
classify(2, False)
classify(3, True)
classify(3, False)
classify(200, True)
classify(200, False)
