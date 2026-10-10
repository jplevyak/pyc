# pyc check driver for rsync (added, not an upstream file: see
# ../PYC_CHANGES.md). rsync's __main__ prints nothing: its result is the
# file it writes. This runs the same steps on the same testdata and prints
# what they computed -- the block signatures, the delta's shape, and the
# patched output -- so it can be compared with CPython's.
# corpus_sweep.sh runs this in place of rsync.py in run/check mode.
import rsync

BLOCKSIZE = 4096


def h(data):
    r = 0
    for v in data:
        r = (r * 31 + v) & 0xffffffff
    return r


unpatched = open("testdata/unpatched.file", "rb")
weak, strong = rsync.blockchecksums(unpatched, BLOCKSIZE)
print("blocks", len(weak), "weak", h(weak) if weak else 0)
for i in range(min(3, len(strong))):
    print("md5", i, strong[i])

patchedfile = open("testdata/patched.file", "rb")
delta = rsync.rsyncdelta(patchedfile, (weak, strong), BLOCKSIZE)
matched = 0
literal = 0
for element in delta:
    if element.index != -1:
        matched += 1
    else:
        literal += len(element.data)
print("delta", len(delta), "matched", matched, "literal bytes", literal)

unpatched.seek(0)
save_to = open("testdata/locally-patched.file", "wb")
rsync.patchstream(unpatched, save_to, delta, BLOCKSIZE)
save_to.close()

result = open("testdata/locally-patched.file", "rb").read()
expected = open("testdata/patched.file", "rb").read()
print("patched", len(result), "bytes, hash", h(result), "equal", result == expected)

# patched.file shares no 4096-byte block with unpatched.file, so the delta
# above is all literal bytes and never takes the block-MATCH path. The delta
# from a file to itself matches every block.
again = open("testdata/unpatched.file", "rb")
delta = rsync.rsyncdelta(again, (weak, strong), BLOCKSIZE)
matched = 0
literal = 0
for element in delta:
    if element.index != -1:
        matched += 1
    else:
        literal += len(element.data)
print("self delta", len(delta), "matched", matched, "literal bytes", literal)
