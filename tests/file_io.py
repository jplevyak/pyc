# File objects: open/write/close, read-all, read(n), readline,
# readlines, and `for line in f` iteration (__pyc__/07_file.py).
def main():
    f = open("file_io.tmp", "w")
    f.write("alpha\n")
    f.write("beta\n")
    f.write("gamma")
    f.close()

    g = open("file_io.tmp")
    print(len(g.read()))
    g.close()

    g = open("file_io.tmp", "r")
    print(g.read(2))
    print(g.readline())
    print(g.readline())
    g.close()

    g = open("file_io.tmp")
    lines = g.readlines()
    g.close()
    print(len(lines))
    print(lines[2])

    count = 0
    total = 0
    for line in open("file_io.tmp"):
        count += 1
        total += len(line)
    print(count)
    print(total)

main()
