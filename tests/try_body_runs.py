# Regression: a `try` statement must not crash the compiler. It was
# routed to the `with`-item handler, which mis-lowered the try body
# into null rvals and aborted if1_send (amaze/go/othello/sudoku3).
# Exception handling is unimplemented (issue 011): the try body runs,
# else/finally run, the except handler is skipped. issue 025 bucket E.
def parse(x):
    try:
        return x + 1
    except:
        return -1

def use_finally(n):
    total = 0
    try:
        total = n * 2
    finally:
        total = total + 1
    return total

def use_else(n):
    try:
        y = n - 1
    except:
        y = 0
    else:
        y = y + 10
    return y

def main():
    print(parse(6))         # 7 (body path)
    print(use_finally(5))   # 11 (body 10, finally +1)
    print(use_else(3))      # 12 (body 2, else +10)

main()
