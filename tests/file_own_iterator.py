# A file is its own iterator, as in CPython (mwmatching's `next(input)`):
# `next(f)`, `for line in f` and `f.readline()` share one position, and
# `next()` past the end raises StopIteration -- for files and for every
# other iterator. pyc's file used to hand `for` a separate read-ahead
# iterator and had no __next__, so `next(f)` did not compile.
w = open("file_own_iterator.txt", "w")
w.write("alpha\nbeta\ngamma\ndelta\nepsilon\n")
w.close()

f = open("file_own_iterator.txt")
print(repr(next(f)))
print(repr(f.readline()))
for line in f:
    print("loop", repr(line))
    if line.startswith("delta"):
        break
print(repr(next(f)))
try:
    next(f)
except StopIteration:
    print("stop")
print(repr(f.readline()), f.tell())
f.seek(0)
print(repr(f.read(3)), repr(next(f)))
print(iter(f) is f)
f.close()

g = open("file_own_iterator.txt", "rb")
print(next(g), g.readline())
for l in g:
    print(l)
print(g.tell())
g.close()

it = iter([1, 2])
print(next(it), next(it))
try:
    next(it)
except StopIteration:
    print("list stop")
