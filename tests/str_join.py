# str.join over a list of strings (issue 025; mandelbrot's
# ''.join(line) was the last blocker to the first green example).
def main():
    parts = ["a", "b", "c"]
    print("".join(parts))
    print("-".join(parts))
    print(", ".join(["one"]))
    print("x".join([]))
    line = []
    for i in range(3):
        line.append("#")
    print("".join(line))

main()
