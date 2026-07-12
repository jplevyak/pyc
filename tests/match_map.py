def classify(val):
    match val:
        case {"x": x, "y": y}:
            print("point:", x, y)
        case {"a": a, "b": b, "c": c} if a > b:
            print("descending triple:", a, b, c)
        case {"a": a, "b": b, "c": c}:
            print("triple:", a, b, c)
        case {}:
            print("empty-map")
        case other:
            print("other:", other)

classify({"x": 1, "y": 2})
classify({"a": 5, "b": 1, "c": 3})
classify({"a": 1, "b": 2, "c": 3})
classify({"x": 1})
classify({})
classify(42)
