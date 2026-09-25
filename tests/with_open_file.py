# issues/170: a file object is its own context manager. `__exit__` closes
# the file -- the reads below see what the writes buffered only because the
# file was flushed and closed at the end of each `with` -- and returns
# False, so an exception raised in the body propagates.
def roundtrip(path):
    with open(path, "w") as f:
        f.write("hello\n")
    with open(path) as f:
        s = f.read()
    return s

class Store:
    def __init__(self, path):
        self.path = path
    def save(self, data):
        with open(self.path, "wb") as f:
            f.write(data)
    def load(self):
        with open(self.path, "rb") as f:
            return f.read()

def write_then_raise(path):
    try:
        with open(path, "w") as f:
            f.write("partial\n")
            raise ValueError("stop")
    except ValueError as e:
        print("caught", e)
    with open(path) as f:
        return f.read()

print(roundtrip("with_open_file.txt"), end="")
st = Store("with_open_file.bin")
st.save(b"bytes\n")
print(st.load() == b"bytes\n")  # compare, not print: bytes repr is issues/051
print(write_then_raise("with_open_file.txt"), end="")
