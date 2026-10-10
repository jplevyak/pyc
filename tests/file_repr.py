# A file object prints as CPython's does: `<_io.BufferedReader name='x'>`
# for binary, `<_io.TextIOWrapper name='x' mode='r' encoding='...'>` for
# text, and str() is the same. It printed `<instance>` (tonyjpegdecoder's
# `'converted %s to %s' % (inputfile, bmpfile)`). The text encoding is the
# locale's, so it is replaced here to keep the expected output portable.
import sys

w = open("file_repr_a.txt", "w")
w.write("hi\n")
w.close()

def text(f):
    return repr(f).replace(repr(f.encoding), "ENC")

fs = [open("file_repr_a.txt"), open("file_repr_o.txt", "w"), open("file_repr_o.txt", "a"),
      open("file_repr_a.txt", "r+")]
for f in fs:
    print(text(f), str(f) == repr(f), "%s" % f == repr(f), f.name, f.mode)
bs = [open("file_repr_a.txt", "rb"), open("file_repr_b.bin", "wb"), open("file_repr_b.bin", "ab"),
      open("file_repr_a.txt", "r+b"), open("file_repr_b.bin", "w+b")]
for f in bs:
    print(repr(f), str(f) == repr(f), f.name, f.mode)
print(repr(fs[0].buffer))
bs[0].close()
print(repr(bs[0]))
print(repr(sys.stdout), repr(sys.stdin), repr(sys.stderr))
print(isinstance(fs[0].encoding, str) and len(fs[0].encoding) > 0)
