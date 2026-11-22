# os's descriptor I/O (open, read, write, close, pipe, openpty, isatty,
# get/set_blocking) and select.select, against CPython: results, readiness,
# timeouts and the errors. Prints no descriptor numbers, which depend on how
# many files the process has open.
import os
import select
import time


def err(f):
    try:
        f()
        print("no error")
    except BlockingIOError as e:
        print("BlockingIOError", e)
    except FileExistsError as e:
        print("FileExistsError", e)
    except FileNotFoundError as e:
        print("FileNotFoundError", e)
    except OSError as e:
        print("OSError", e)
    except ValueError as e:
        print("ValueError", e)
    except TypeError as e:
        print("TypeError", e)


class Wrapped:
    def __init__(self, fd):
        self.fd = fd

    def fileno(self):
        return self.fd


r, w = os.pipe()
print(os.isatty(r), os.get_blocking(r))
print(select.select([r], [w], [r], 0) == ([], [w], []))
print(os.write(w, b"hello"))
rr, ww, xx = select.select([r, r], [], [], 0.5)
print(len(rr), rr[0] == r, rr[1] == r, ww, xx)
print(os.read(r, 3), os.read(r, 10))
wr = Wrapped(r)
print(select.select([wr], [], [], 0)[0] == [])
os.write(w, bytearray(b"xy"))
got = select.select([wr], [], [], 1.0)[0]
print(len(got), got[0] is wr, os.read(r, 0), os.read(r, 5))

os.set_blocking(r, False)
print(os.get_blocking(r))
err(lambda: os.read(r, 1))
os.set_blocking(r, True)

t = time.monotonic()
print(select.select([r], [], [], 0.2))
d = time.monotonic() - t
print(d >= 0.2, d < 2.0)
t = time.monotonic()
print(select.select([], [], [], 0.1))
d = time.monotonic() - t
print(d >= 0.1, d < 2.0)

os.close(w)
print(select.select([r], [], [], None)[0] == [r], os.read(r, 5))
os.close(r)
err(lambda: os.close(r))
err(lambda: os.read(r, 1))
err(lambda: select.select([r], [], [], 0))

r2, w2 = os.pipe()
err(lambda: os.write(w2, "text"))
err(lambda: os.write(w2, 5))
err(lambda: os.read(r2, -1))
err(lambda: select.select([], [], [], -1))
err(lambda: select.select([-1], [], [], 0))
err(lambda: select.select([5000], [], [], 0))
os.close(r2)
os.close(w2)

name = "os_fd_select.tmp"
fd = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
print(os.write(fd, b"line one\nline two\n"))
os.close(fd)
err(lambda: os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL))
err(lambda: os.open("no/such/dir/file", os.O_RDONLY))
fd = os.open(name, os.O_RDONLY)
print(os.read(fd, 100), os.read(fd, 100))
os.close(fd)
fd = os.open(name, os.O_WRONLY | os.O_APPEND)
os.write(fd, b"three\n")
os.close(fd)
fd = os.open(name, os.O_RDONLY)
print(os.read(fd, 100))
os.close(fd)
os.remove(name)

m, s = os.openpty()
print(os.isatty(s), os.isatty(m))
os.write(m, b"typed\n")
print(select.select([s], [], [], 1.0)[0] == [s], os.read(s, 100))
os.close(s)
os.close(m)
