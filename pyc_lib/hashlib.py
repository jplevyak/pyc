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
