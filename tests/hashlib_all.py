# Every algorithm in hashlib.algorithms_guaranteed against CPython, over
# lengths that straddle each block size (64, 72, 104, 128, 136, 144, 168),
# one-shot and in pieces, plus copy(), new(), blake2 parameters, shake
# lengths, pbkdf2_hmac and the error messages.
import hashlib
import os

names = sorted(hashlib.algorithms_guaranteed)
lengths = [0, 1, 3, 55, 56, 63, 64, 65, 71, 72, 103, 104, 111, 112, 127, 128, 129, 135, 136, 143, 144, 167, 168, 169, 300, 1000]
for name in names:
    for n in lengths:
        data = bytes([(i * 7 + n) % 256 for i in range(n)])
        h = hashlib.new(name, data)
        g = hashlib.new(name)
        g.update(data[: n // 3])
        g.update(data[n // 3:])
        if name.startswith("shake"):
            a = h.hexdigest(n % 70 + 1)
            b = g.hexdigest(n % 70 + 1)
        else:
            a = h.hexdigest()
            b = g.hexdigest()
        print(name, n, a, a == b)
    h = hashlib.new(name, b"abc")
    print(h.name, h.digest_size, h.block_size)

h = hashlib.sha256(b"ab")
c = h.copy()
c.update(b"c")
print(h.hexdigest(), c.hexdigest(), c.hexdigest() == hashlib.sha256(b"abc").hexdigest())
print(hashlib.new("SHA256", b"x").hexdigest(), hashlib.sha1(bytearray(b"bytes")).hexdigest())
print(len(hashlib.sha512(b"x").digest()), hashlib.md5(b"x", usedforsecurity=False).hexdigest())

# A digest and a copy taken while the input is still short, then updates
# that carry both past the runtime's 256-byte prefix (pyc_runtime.c).
for name in ["md5", "sha256", "sha512", "sha3_256", "shake_128"]:
    h = hashlib.new(name, b"p" * 200)
    early = h.hexdigest(16) if name == "shake_128" else h.hexdigest()
    c = h.copy()
    h.update(b"q" * 100)
    c.update(b"r" * 1000)
    h.update(b"s")
    if name == "shake_128":
        print(name, early, h.hexdigest(16), c.hexdigest(16))
    else:
        print(name, early, h.hexdigest(), c.hexdigest())

print(hashlib.blake2b(b"x", digest_size=8).hexdigest())
print(hashlib.blake2b(b"msg", key=b"secret", salt=b"salty", person=b"me").hexdigest())
print(hashlib.blake2s(b"msg", digest_size=20, key=b"k" * 32, salt=b"12345678", person=b"abcdefgh").hexdigest())
print(hashlib.blake2b(b"", key=b"k").hexdigest(), hashlib.blake2s(key=b"k").hexdigest())
print(hashlib.blake2b(b"z" * 128, key=b"key").hexdigest())
print(hashlib.shake_256(b"abc").hexdigest(500)[-16:], len(hashlib.shake_128(b"").digest(0)))

print(hashlib.pbkdf2_hmac("sha256", b"password", b"salt", 4096).hex())
print(hashlib.pbkdf2_hmac("sha1", b"password", b"salt", 2, 50).hex())
print(hashlib.pbkdf2_hmac("sha512", b"p" * 200, b"", 3).hex())
print(hashlib.pbkdf2_hmac("sha3_256", b"pw", b"s", 5, 10).hex())
print(hashlib.pbkdf2_hmac("blake2b", b"pw", b"s", 2).hex())

def err(f):
    try:
        f()
        print("no error")
    except TypeError as e:
        print("TypeError", e)
    except ValueError as e:
        print("ValueError", e)

err(lambda: hashlib.new("foo"))
err(lambda: hashlib.sha256("abc"))
err(lambda: hashlib.sha256().digest(3))
err(lambda: hashlib.blake2b().hexdigest(3))
err(lambda: hashlib.shake_128().digest())
err(lambda: hashlib.shake_256().hexdigest())
err(lambda: hashlib.blake2b(digest_size=65))
err(lambda: hashlib.blake2s(digest_size=0))
err(lambda: hashlib.blake2b(key=b"x" * 65))
err(lambda: hashlib.blake2b(salt=b"x" * 17))
err(lambda: hashlib.blake2s(person=b"x" * 9))
err(lambda: hashlib.pbkdf2_hmac("sha256", b"p", b"s", 0))
err(lambda: hashlib.pbkdf2_hmac("sha256", b"p", b"s", 1, 0))

f = open("hashlib_all.tmp", "wb")
f.write(bytes([i % 251 for i in range(600000)]))
f.close()
f = open("hashlib_all.tmp", "rb")
print(hashlib.file_digest(f, "sha256").hexdigest())
f.close()
os.remove("hashlib_all.tmp")
