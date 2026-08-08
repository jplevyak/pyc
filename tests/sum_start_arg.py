# issues/025 TODO item 4: sum() only ever accepted one argument,
# hardcoding an int(0) accumulator seed -- the 2-arg form (`start`)
# is needed for the common list-flatten idiom.
print(sum([1, 2, 3]))
print(sum([1, 2, 3], 10))
print(sum([1.5, 2.5], 0.0))
print(sum([[1, 2], [3, 4], [5, 6]], []))
