# ifa/issues/041: assessed for a real implementation (2026-08-11),
# deferred -- silently-wrong stub (update() is a no-op, digest()/
# hexdigest() always ""), found via the same audit that fixed getopt/
# os/string/sys. Real corpus usage (shedskin_examples/rsync/rsync.py,
# the only corpus user, needs md5 only) is
# `hashlib.md5(bytes(a_deque_of_ints)).hexdigest()` -- blocked before
# a hash algorithm would even run by the same `bytes(iterable)`
# construction gap struct.py's deferral note documents (confirmed:
# `bytes(x)` where x is a deque/bytearray of ints resolves to
# "expression has no type"). Implementing a real MD5 (a well-defined,
# moderate ~80-line algorithm, not the hard part) is worth doing once
# that construction path exists; not attempted here since it isn't
# reachable from real usage yet.

class _HashStub:
    def update(self, arg):
        pass
    def digest(self):
        return ""
    def hexdigest(self):
        return ""

def md5(arg=None):
    return _HashStub()

def sha1(arg=None):
    return _HashStub()

def sha224(arg=None):
    return _HashStub()

def sha256(arg=None):
    return _HashStub()

def sha384(arg=None):
    return _HashStub()

def sha512(arg=None):
    return _HashStub()
