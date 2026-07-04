def test_match(val):
    match val:
        case 1:
            print("one")
        case 2:
            print("two")
        case _:
            print("default")

test_match(1)
test_match(2)
test_match(3)
