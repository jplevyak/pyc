# print()'s keyword arguments were DROPPED.
#
# python_ifa_build_if1.cc's sym_print branch only ever looked at the
# POSITIONAL arguments, so `sep=`, `end=` and `file=` were silently
# ignored: every print went to stdout, space-separated, newline-terminated.
#
# `file=` is the damaging one. shedskin_examples/hq2x writes its entire
# PPM with `print(r, g, b, file=f)`, so the image went to the TERMINAL
# (1.4 MB of it) and randam2.ppm was left empty -- while the compile
# reported nothing at all, on either backend. That is the worst available
# outcome: no warning, exit 0, and a silently wrong result.
#
# `flush=` is accepted and ignored (nothing is buffered to flush).
# An unknown keyword is now an error rather than being dropped.
print("a", "b", "c", sep="-")
print("x", end="")
print("y", end="!\n")
print(1, 2, sep="", end="|")
print()

f = open("print_keyword_args.tmp", "w")
print("p", "q", sep="+", end=";", file=f)
print("r", file=f)
print(3, 4, file=f)
f.close()

# Read it back with .read()+.split(), not repr() and not `for line in
# fileobj`: repr() does not escape a newline here (issues/051 is the
# bytes half of that) and iterating a file object directly is a separate
# gap -- neither is what this test is about.
for line in open("print_keyword_args.tmp").read().split("\n"):
    if line:
        print("FILE:", line)

print("flush ok", flush=True)
