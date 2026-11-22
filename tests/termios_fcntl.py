# termios and fcntl over a pseudo-terminal, against CPython: attributes,
# speeds, non-canonical reads, window size, flush, and the errors. Reads cc
# only at VMIN and VTIME in non-canonical mode, where CPython's elements
# are ints too, so the output is CPython's although pyc's tcgetattr returns
# every cc element as an int (PYC_TERMIOS_CC_INT, which this file's .check
# shows announcing itself at each tcgetattr call).
import fcntl
import os
import select
import struct
import termios


def err(f):
    try:
        f()
        print("no error")
    except termios.error as e:
        print("termios.error", e)
    except OverflowError as e:
        print("OverflowError", e)
    except OSError as e:
        print("OSError", e)
    except ValueError as e:
        print("ValueError", e)
    except TypeError as e:
        print("TypeError", e)


m, s = os.openpty()
a = termios.tcgetattr(s)
print(len(a), len(a[6]) == termios.NCCS)

a[3] = a[3] & ~(termios.ICANON | termios.ECHO)
a[6][termios.VMIN] = 0
a[6][termios.VTIME] = 0
a[4] = termios.B9600
a[5] = termios.B9600
termios.tcsetattr(s, termios.TCSANOW, a)
b = termios.tcgetattr(s)
print(b[3] & termios.ICANON, b[3] & termios.ECHO, b[6][termios.VMIN], b[6][termios.VTIME])
print(b[4] == termios.B9600, b[5] == termios.B9600)

os.write(m, b"xy")
print(select.select([s], [], [], 1.0)[0] == [s])
print(struct.unpack("i", fcntl.ioctl(s, termios.FIONREAD, struct.pack("i", 0)))[0])
print(os.read(s, 10))

os.write(m, b"junk")
select.select([s], [], [], 1.0)
termios.tcflush(s, termios.TCIFLUSH)
print(select.select([s], [], [], 0)[0])
termios.tcdrain(s)
termios.tcflow(s, termios.TCOON)
termios.tcsendbreak(s, 0)

termios.tcsetwinsize(s, (24, 80))
print(termios.tcgetwinsize(s), termios.tcgetwinsize(m))

c = termios.tcgetattr(s)
termios.tcsetattr(s, termios.TCSADRAIN, [c[0], c[1], c[2], c[3], c[4], c[5], [b"\x02"] * termios.NCCS])
print(termios.tcgetattr(s)[6][termios.VMIN])

r, w = os.pipe()
err(lambda: termios.tcgetattr(r))
err(lambda: termios.tcsetattr(s, termios.TCSANOW, [0, 0, 0, 0, 0, 0]))
err(lambda: termios.tcsetattr(s, termios.TCSANOW, [c[0], c[1], c[2], c[3], c[4], c[5], [0, 0, 0]]))
err(lambda: termios.tcsetattr(s, termios.TCSANOW, [c[0], c[1], c[2], c[3], c[4], c[5], [b"ab"] * termios.NCCS]))
err(lambda: termios.tcsetattr(s, 99, c))
err(lambda: termios.tcflush(s, 99))
err(lambda: termios.tcdrain(r))
err(lambda: termios.tcsetwinsize(s, (70000, 1)))

print(fcntl.fcntl(r, fcntl.F_GETFD) & fcntl.FD_CLOEXEC)
print(fcntl.fcntl(r, fcntl.F_SETFL, os.O_NONBLOCK), fcntl.fcntl(r, fcntl.F_GETFL) & os.O_NONBLOCK == os.O_NONBLOCK)
fcntl.flock(r, fcntl.LOCK_EX | fcntl.LOCK_NB)
fcntl.flock(r, fcntl.LOCK_UN)
os.write(w, b"abc")
print(fcntl.ioctl(r, termios.FIONREAD, "abcd"))
err(lambda: fcntl.ioctl(99, termios.FIONREAD, 0))
err(lambda: fcntl.ioctl(r, termios.FIONREAD, b"x" * 1025))
err(lambda: fcntl.fcntl(r, fcntl.F_GETFL, b"x" * 1025))
err(lambda: fcntl.ioctl(s, termios.TIOCMGET, struct.pack("I", 0)))
for fd in [r, w, s, m]:
    os.close(fd)
