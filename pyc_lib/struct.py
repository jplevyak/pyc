def pack(fmt, *args):
    # No-op stub (issues/025 -- struct format-string parsing is a
    # separate, sizeable follow-up). CPython's struct.pack always
    # returns bytes, never str; matching that type contract (even
    # though the stub doesn't actually pack anything) keeps callers
    # that concat the result with other bytes values type-consistent.
    return b""

def unpack(fmt, string):
    return ()

def unpack_from(fmt, string, offset=0):
    return ()

def calcsize(fmt):
    return 0
