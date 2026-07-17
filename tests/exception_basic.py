# issue 011 (option C): try/except/raise basics. Bare except catches
# any exception; a typed clause matches by class; the else-clause
# runs only when the try body raised nothing.
def risky(n):
    if n > 5:
        raise ValueError("too big")
    return n * 2

def bare(n):
    try:
        v = risky(n)
    except:
        v = -1
    return v

def typed(n):
    try:
        v = risky(n)
    except ValueError as e:
        print("caught: " + str(e))
        v = -1
    return v

def with_else(n):
    try:
        v = risky(n)
    except ValueError:
        v = -1
    else:
        v = v + 100
    return v

print(bare(3))
print(bare(9))
print(typed(3))
print(typed(9))
print(with_else(3))
print(with_else(9))
