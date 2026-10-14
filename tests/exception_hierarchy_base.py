# Exception derives from BaseException, as in CPython. It used to derive
# from object, so `except BaseException` missed every Exception subclass
# and the program died with "Unhandled exception". The flat hierarchy
# also made the unhandled-exception printer's e.__str__() a run-time
# dispatch between two identical bodies (ifa/190): with one __str__ for
# every builtin exception, it is a null test plus a direct call.
def risky(n):
    if n > 5:
        raise ValueError("too big")
    if n < 0:
        raise IndexError("negative")
    return n * 2

def guarded(n):
    try:
        return risky(n)
    except BaseException as e:
        print("caught: " + str(e))
        return -1

print(guarded(3))
print(guarded(9))
print(guarded(-4))
print(isinstance(ValueError("x"), BaseException))
print(isinstance(SystemExit(0), Exception))
# A module-level call outside any try routes to the unhandled-exception
# printer, so the printer is compiled; this one does not raise.
print(risky(1))
