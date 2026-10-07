# ifa/issues/049: a function that can only raise produces no value, and
# code after a call to it does not run -- so the call's result is not a
# type error. `header` uses read's result and returns a tuple that `run`
# unpacks; neither line can execute, and pyc refused the program with
# "'a' has no type" (msp_ss's serial reads, once serial.py raises).
# The same function called with an argument that returns must still give
# the real value.
class PortError(Exception):
    pass


def read(n):
    raise PortError("no port")


def header():
    h = read(1)
    return h[0] & 0xf0, h[0] & 0x0f


def run():
    a, b = header()
    print(a, b)


def risky(n):
    if n > 5:
        raise ValueError("too big")
    return n


try:
    run()
except PortError as e:
    print("caught", e)
try:
    print(risky(9))
except ValueError as e:
    print("caught", e)
print(risky(2))
print("done")
