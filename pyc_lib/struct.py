# ifa/issues/041: assessed for a real implementation (2026-08-11),
# deferred -- not because the format-string parsing/bit-packing logic
# is hard (it isn't: corpus usage only needs B/H/I with </> endianness
# plus x padding-skip, ordinary integer arithmetic, no floats/doubles),
# but because it's blocked on two things that are compiler/runtime
# feature gaps, not library-shim work:
#   1. `*args` in a function definition is parsed but not compiled
#      (ROADMAP.md "Phase 6.1"; confirmed with a minimal repro this
#      session -- any call through a *args-taking function currently
#      fails with "matching function not found"). struct.pack(fmt, a,
#      b, c, ...)'s real signature needs this.
#   2. Building a `bytes` value from a computed sequence of integer
#      byte values (e.g. `bytes(a_bytearray)` or `bytes([65, 66])`)
#      doesn't resolve at all ("expression has no type") -- confirmed
#      with a minimal repro. pyc's `bytearray` is also a fixed-size
#      @vector type (no .append()), not the growable buffer CPython's
#      struct.pack builds up internally, so even a *args-free rewrite
#      still needs this conversion to exist.
# Revisit once either lands; the parsing/packing logic itself is
# straightforward pure-Python work at that point.

def pack(fmt, *args):
    # CPython's struct.pack always returns bytes, never str; matching
    # that type contract (even though the stub doesn't actually pack
    # anything) keeps callers that concat the result with other bytes
    # values type-consistent.
    return b""

def unpack(fmt, string):
    return ()

def unpack_from(fmt, string, offset=0):
    return ()

def calcsize(fmt):
    return 0
