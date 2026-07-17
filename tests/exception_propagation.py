# issue 011: an exception raised deep inside a call chain propagates
# through intermediate frames (no handler there) up to the first
# enclosing try, even across multiple function boundaries.
def risky(n):
    if n > 5:
        raise ValueError("too big")
    return n

def middle(n):
    v = risky(n)  # exception must pass through this frame untouched
    return v + 1

def outer(n):
    v = middle(n)
    return v * 2

def run(n):
    try:
        return outer(n)
    except ValueError as e:
        return -1

print(run(3))
print(run(9))

# finally runs on both the exception and no-exception paths, and
# still lets an unhandled exception re-propagate afterward.
def with_finally(n):
    try:
        v = risky(n)
    finally:
        print("cleanup")
    return v

print(with_finally(3))
try:
    print(with_finally(9))
except ValueError as e:
    print("outer caught: " + str(e))

# bare `raise` inside a handler re-raises the current exception.
def reraise(n):
    try:
        return risky(n)
    except ValueError:
        print("logging")
        raise

try:
    print(reraise(9))
except ValueError as e:
    print("final: " + str(e))
