# hashlib.md5 is a real MD5 (RFC 1321): rsync compares md5 hexdigests, and
# the stub returned "" for every digest. Covers the padding boundaries
# (55/56/64 bytes), a multi-block input and incremental update().
import hashlib
for d in [b"", b"a", b"abc", b"message digest", b"x" * 55, b"y" * 56, b"z" * 64, bytes(list(range(256))) * 3]:
    print(hashlib.md5(d).hexdigest())
h = hashlib.md5(b"ab")
h.update(b"c")
print(h.hexdigest(), len(h.digest()))
