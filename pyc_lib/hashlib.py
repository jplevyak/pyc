# pyc shim for `hashlib`: every algorithm in CPython's
# `algorithms_guaranteed` (md5, sha1, sha224/256/384/512, sha3_224/256/384/512,
# shake_128/256, blake2b/s), plus new(), pbkdf2_hmac() and file_digest().
#
# The digests are C, in pyc_runtime.c (`_CG_hash_*`): OpenSSL's libcrypto,
# loaded at run time when present (its SHA-NI/AVX2 code, as CPython's hashlib
# uses), and otherwise built-in C. The built-in code is C because SHA-512/384,
# Keccak and BLAKE2b work in unsigned 64-bit words with wraparound, which
# pyc's int64 cannot express with CPython semantics (issues/177).
# PYC_HASHLIB_BUILTIN=1 forces the built-in code. A hash's state is a bytes
# value the hash object owns and the C updates in place; copy() duplicates it.
#
# One class serves every algorithm, so hashlib.new() returns one type
# whatever the name. Not provided: scrypt and the OpenSSL-only algorithms in
# CPython's algorithms_available (sha512_256, ripemd160, ...), which are a
# missing name or "unsupported hash type", never a wrong digest.

_MD5 = 0
_SHA1 = 1
_SHA224 = 2
_SHA256 = 3
_SHA384 = 4
_SHA512 = 5
_SHA3_224 = 6
_SHA3_256 = 7
_SHA3_384 = 8
_SHA3_512 = 9
_SHAKE_128 = 10
_SHAKE_256 = 11
_BLAKE2B = 12
_BLAKE2S = 13

_NAMES = ["md5", "sha1", "sha224", "sha256", "sha384", "sha512",
          "sha3_224", "sha3_256", "sha3_384", "sha3_512", "shake_128", "shake_256",
          "blake2b", "blake2s"]
_DIGEST_SIZES = [16, 20, 28, 32, 48, 64, 28, 32, 48, 64, 0, 0, 64, 32]
_BLOCK_SIZES = [64, 64, 64, 64, 128, 128, 144, 136, 104, 72, 168, 136, 128, 64]

algorithms_guaranteed = {"md5", "sha1", "sha224", "sha256", "sha384", "sha512",
                         "sha3_224", "sha3_256", "sha3_384", "sha3_512", "shake_128", "shake_256",
                         "blake2b", "blake2s"}
algorithms_available = {"md5", "sha1", "sha224", "sha256", "sha384", "sha512",
                        "sha3_224", "sha3_256", "sha3_384", "sha3_512", "shake_128", "shake_256",
                        "blake2b", "blake2s"}


def _buffer(data):
    if isinstance(data, str):
        raise TypeError("Strings must be encoded before hashing")
    return bytes(data)


def _state(alg, digest_size, key, salt, person):
    return __pyc_c_call__(bytes, "_CG_hash_new", int, alg, int, digest_size, bytes, key, bytes, salt,
                          bytes, person)


class _Hash:
    def __init__(self, alg, state, digest_size):
        self._alg = alg
        self._state = state
        self.name = _NAMES[alg]
        self.digest_size = digest_size
        self.block_size = _BLOCK_SIZES[alg]

    def update(self, data):
        __pyc_c_call__(int, "_CG_hash_update", bytes, self._state, bytes, _buffer(data))

    def _length(self, length, method):
        if self._alg == _SHAKE_128 or self._alg == _SHAKE_256:
            if length is None:
                raise TypeError(method + "() missing required argument 'length' (pos 1)")
            if length < 0:
                # CPython raises SystemError here; pyc has no SystemError.
                raise ValueError("length must be non-negative")
            return length
        if length is not None:
            owner = "HASH"
            if self._alg == _BLAKE2B or self._alg == _BLAKE2S:
                owner = self.name
            raise TypeError(owner + "." + method + "() takes no arguments (1 given)")
        return self.digest_size

    def digest(self, length=None):
        n = self._length(length, "digest")
        return __pyc_c_call__(bytes, "_CG_hash_digest", bytes, self._state, int, n)

    def hexdigest(self, length=None):
        n = self._length(length, "hexdigest")
        d = __pyc_c_call__(bytes, "_CG_hash_digest", bytes, self._state, int, n)
        return __pyc_c_call__(str, "_CG_bytes_hex", bytes, d)

    def copy(self):
        return _Hash(self._alg, __pyc_c_call__(bytes, "_CG_hash_copy", bytes, self._state), self.digest_size)


def _make(alg, data):
    h = _Hash(alg, _state(alg, _DIGEST_SIZES[alg], b"", b"", b""), _DIGEST_SIZES[alg])
    h.update(data)
    return h


def _blake2(alg, data, digest_size, key, salt, person):
    limit = _DIGEST_SIZES[alg]
    if digest_size < 1 or digest_size > limit:
        raise ValueError("digest_size must be between 1 and " + str(limit) + " bytes")
    key = _buffer(key)
    salt = _buffer(salt)
    person = _buffer(person)
    if len(key) > limit:
        raise ValueError("maximum key length is " + str(limit) + " bytes")
    if len(salt) > limit // 4:
        raise ValueError("maximum salt length is " + str(limit // 4) + " bytes")
    if len(person) > limit // 4:
        raise ValueError("maximum person length is " + str(limit // 4) + " bytes")
    h = _Hash(alg, _state(alg, digest_size, key, salt, person), digest_size)
    h.update(data)
    return h


def md5(data=b"", *, usedforsecurity=True):
    return _make(_MD5, data)


def sha1(data=b"", *, usedforsecurity=True):
    return _make(_SHA1, data)


def sha224(data=b"", *, usedforsecurity=True):
    return _make(_SHA224, data)


def sha256(data=b"", *, usedforsecurity=True):
    return _make(_SHA256, data)


def sha384(data=b"", *, usedforsecurity=True):
    return _make(_SHA384, data)


def sha512(data=b"", *, usedforsecurity=True):
    return _make(_SHA512, data)


def sha3_224(data=b"", *, usedforsecurity=True):
    return _make(_SHA3_224, data)


def sha3_256(data=b"", *, usedforsecurity=True):
    return _make(_SHA3_256, data)


def sha3_384(data=b"", *, usedforsecurity=True):
    return _make(_SHA3_384, data)


def sha3_512(data=b"", *, usedforsecurity=True):
    return _make(_SHA3_512, data)


def shake_128(data=b"", *, usedforsecurity=True):
    return _make(_SHAKE_128, data)


def shake_256(data=b"", *, usedforsecurity=True):
    return _make(_SHAKE_256, data)


def blake2b(data=b"", *, digest_size=64, key=b"", salt=b"", person=b"", usedforsecurity=True):
    return _blake2(_BLAKE2B, data, digest_size, key, salt, person)


def blake2s(data=b"", *, digest_size=32, key=b"", salt=b"", person=b"", usedforsecurity=True):
    return _blake2(_BLAKE2S, data, digest_size, key, salt, person)


# CPython 3.12 resolves a name through OpenSSL first, which also accepts the
# md5/sha1/sha2 names in any case ("SHA256"); the others are exact.
def _lookup(name):
    for i in range(len(_NAMES)):
        if _NAMES[i] == name:
            return i
    lower = name.lower()
    for i in range(_SHA512 + 1):
        if _NAMES[i] == lower:
            return i
    raise ValueError("unsupported hash type " + name)


def new(name, data=b"", *, usedforsecurity=True):
    alg = _lookup(name)
    if alg == _BLAKE2B or alg == _BLAKE2S:
        return _blake2(alg, data, _DIGEST_SIZES[alg], b"", b"", b"")
    return _make(alg, data)


def pbkdf2_hmac(hash_name, password, salt, iterations, dklen=None):
    alg = _lookup(hash_name)
    password = _buffer(password)
    salt = _buffer(salt)
    if alg == _SHAKE_128 or alg == _SHAKE_256:
        raise ValueError("unsupported hash type " + hash_name)
    if iterations < 1:
        raise ValueError("iteration value must be greater than 0.")
    n = _DIGEST_SIZES[alg]
    if dklen is not None:
        n = dklen
    if n < 1:
        raise ValueError("key length must be greater than 0.")
    return __pyc_c_call__(bytes, "_CG_hash_pbkdf2", int, alg, bytes, password, bytes, salt, int, iterations,
                          int, n)


# CPython's file_digest takes a name or a constructor; this one takes a name.
# The file must be open in binary mode.
def file_digest(fileobj, digest):
    h = new(digest)
    while True:
        chunk = fileobj.read(262144)
        if not chunk:
            break
        h.update(chunk)
    return h
