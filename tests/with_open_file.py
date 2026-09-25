# issues/170: a file object is not a context manager -- __pyc_file__ has no
# __enter__/__exit__, so `with open(...) as f` leaves `f` untyped.
def roundtrip(path):
    with open(path, "w") as f:
        f.write("hello\n")
    with open(path) as f:
        s = f.read()
    return s
print(roundtrip("with_open_file.txt"), end="")
