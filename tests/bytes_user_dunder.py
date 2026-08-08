# issues/025 TODO item 5: bytes(x) dispatched to __pyc_tobytes__ only
# (bytes/str/list's internal name), never CPython's real __bytes__
# dunder -- a plain user class defining __bytes__ had no way in.
class Thing:
    def __init__(self, data):
        self.data = data
    def __bytes__(self):
        return b"BM" + self.data

t = Thing(b"hello")
print(bytes(t))

# bytes(bytes_value) -- the identity case; on the LLVM backend this
# separately needed pyc_runtime.c's missing _CG_string_identity
# extern declaration (undefined reference at link time otherwise).
print(bytes(b"already bytes"))
