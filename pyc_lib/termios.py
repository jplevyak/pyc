# pyc shim for the standard `termios` module: tcgetattr, tcsetattr,
# tcsendbreak, tcdrain, tcflush, tcflow, tcgetwinsize and tcsetwinsize, with
# CPython's results and errors (termios.error, whose message is CPython's
# "(errno, 'strerror')"). The work is C (`_CG_tc_*`, pyc_runtime.c), where a
# struct termios has the platform's layout; the constants come from C too.
# Only constants present on both Linux and macOS are defined, so a name is
# never a value CPython would not have.
#
# ONE ACCOMMODATION (issues/171), PYC_TERMIOS_CC_INT: tcgetattr's cc list
# holds ints. CPython's holds a 1-byte bytes per element, except VMIN and
# VTIME in non-canonical mode, which are ints: a list of two types, which
# pyc cannot represent. Permissive mode returns every element as its int
# value, and says so at each call; --strict refuses tcgetattr. tcsetattr
# takes cc elements of either kind, as CPython does, so a program that only
# assigns VMIN/VTIME and passes the list back behaves as under CPython.
#
# The returned list is CPython's [iflag, oflag, cflag, lflag, ispeed,
# ospeed, cc]. pyc types it by position, so indexing and assigning at a
# constant index work; copying it whole (list(a), a[:]) is refused at
# compile time. Call tcgetattr again for a second copy.

import os


class error(Exception):
    pass


def _fail():
    e = __pyc_c_call__(int, "_CG_os_err")
    raise error("(" + str(e) + ", " + repr(os.strerror(e)) + ")")


def _fd(fd):
    if isinstance(fd, int):
        return fd
    return fd.fileno()


# tcsetattr when
TCSANOW = __pyc_c_call__(int, "_CG_termios_const", int, 0)
TCSADRAIN = __pyc_c_call__(int, "_CG_termios_const", int, 1)
TCSAFLUSH = __pyc_c_call__(int, "_CG_termios_const", int, 2)
# tcflush queue
TCIFLUSH = __pyc_c_call__(int, "_CG_termios_const", int, 3)
TCOFLUSH = __pyc_c_call__(int, "_CG_termios_const", int, 4)
TCIOFLUSH = __pyc_c_call__(int, "_CG_termios_const", int, 5)
# tcflow action
TCOOFF = __pyc_c_call__(int, "_CG_termios_const", int, 6)
TCOON = __pyc_c_call__(int, "_CG_termios_const", int, 7)
TCIOFF = __pyc_c_call__(int, "_CG_termios_const", int, 8)
TCION = __pyc_c_call__(int, "_CG_termios_const", int, 9)
# baud rates
B0 = __pyc_c_call__(int, "_CG_termios_const", int, 10)
B50 = __pyc_c_call__(int, "_CG_termios_const", int, 11)
B75 = __pyc_c_call__(int, "_CG_termios_const", int, 12)
B110 = __pyc_c_call__(int, "_CG_termios_const", int, 13)
B134 = __pyc_c_call__(int, "_CG_termios_const", int, 14)
B150 = __pyc_c_call__(int, "_CG_termios_const", int, 15)
B200 = __pyc_c_call__(int, "_CG_termios_const", int, 16)
B300 = __pyc_c_call__(int, "_CG_termios_const", int, 17)
B600 = __pyc_c_call__(int, "_CG_termios_const", int, 18)
B1200 = __pyc_c_call__(int, "_CG_termios_const", int, 19)
B1800 = __pyc_c_call__(int, "_CG_termios_const", int, 20)
B2400 = __pyc_c_call__(int, "_CG_termios_const", int, 21)
B4800 = __pyc_c_call__(int, "_CG_termios_const", int, 22)
B9600 = __pyc_c_call__(int, "_CG_termios_const", int, 23)
B19200 = __pyc_c_call__(int, "_CG_termios_const", int, 24)
B38400 = __pyc_c_call__(int, "_CG_termios_const", int, 25)
B57600 = __pyc_c_call__(int, "_CG_termios_const", int, 26)
B115200 = __pyc_c_call__(int, "_CG_termios_const", int, 27)
B230400 = __pyc_c_call__(int, "_CG_termios_const", int, 28)
# iflag
IGNBRK = __pyc_c_call__(int, "_CG_termios_const", int, 29)
BRKINT = __pyc_c_call__(int, "_CG_termios_const", int, 30)
IGNPAR = __pyc_c_call__(int, "_CG_termios_const", int, 31)
PARMRK = __pyc_c_call__(int, "_CG_termios_const", int, 32)
INPCK = __pyc_c_call__(int, "_CG_termios_const", int, 33)
ISTRIP = __pyc_c_call__(int, "_CG_termios_const", int, 34)
INLCR = __pyc_c_call__(int, "_CG_termios_const", int, 35)
IGNCR = __pyc_c_call__(int, "_CG_termios_const", int, 36)
ICRNL = __pyc_c_call__(int, "_CG_termios_const", int, 37)
IXON = __pyc_c_call__(int, "_CG_termios_const", int, 38)
IXANY = __pyc_c_call__(int, "_CG_termios_const", int, 39)
IXOFF = __pyc_c_call__(int, "_CG_termios_const", int, 40)
IMAXBEL = __pyc_c_call__(int, "_CG_termios_const", int, 41)
# oflag
OPOST = __pyc_c_call__(int, "_CG_termios_const", int, 42)
ONLCR = __pyc_c_call__(int, "_CG_termios_const", int, 43)
OCRNL = __pyc_c_call__(int, "_CG_termios_const", int, 44)
ONOCR = __pyc_c_call__(int, "_CG_termios_const", int, 45)
ONLRET = __pyc_c_call__(int, "_CG_termios_const", int, 46)
# cflag
CSIZE = __pyc_c_call__(int, "_CG_termios_const", int, 47)
CS5 = __pyc_c_call__(int, "_CG_termios_const", int, 48)
CS6 = __pyc_c_call__(int, "_CG_termios_const", int, 49)
CS7 = __pyc_c_call__(int, "_CG_termios_const", int, 50)
CS8 = __pyc_c_call__(int, "_CG_termios_const", int, 51)
CSTOPB = __pyc_c_call__(int, "_CG_termios_const", int, 52)
CREAD = __pyc_c_call__(int, "_CG_termios_const", int, 53)
PARENB = __pyc_c_call__(int, "_CG_termios_const", int, 54)
PARODD = __pyc_c_call__(int, "_CG_termios_const", int, 55)
HUPCL = __pyc_c_call__(int, "_CG_termios_const", int, 56)
CLOCAL = __pyc_c_call__(int, "_CG_termios_const", int, 57)
CRTSCTS = __pyc_c_call__(int, "_CG_termios_const", int, 58)
# lflag
ISIG = __pyc_c_call__(int, "_CG_termios_const", int, 59)
ICANON = __pyc_c_call__(int, "_CG_termios_const", int, 60)
ECHO = __pyc_c_call__(int, "_CG_termios_const", int, 61)
ECHOE = __pyc_c_call__(int, "_CG_termios_const", int, 62)
ECHOK = __pyc_c_call__(int, "_CG_termios_const", int, 63)
ECHONL = __pyc_c_call__(int, "_CG_termios_const", int, 64)
NOFLSH = __pyc_c_call__(int, "_CG_termios_const", int, 65)
TOSTOP = __pyc_c_call__(int, "_CG_termios_const", int, 66)
IEXTEN = __pyc_c_call__(int, "_CG_termios_const", int, 67)
ECHOCTL = __pyc_c_call__(int, "_CG_termios_const", int, 68)
ECHOKE = __pyc_c_call__(int, "_CG_termios_const", int, 69)
ECHOPRT = __pyc_c_call__(int, "_CG_termios_const", int, 70)
FLUSHO = __pyc_c_call__(int, "_CG_termios_const", int, 71)
PENDIN = __pyc_c_call__(int, "_CG_termios_const", int, 72)
# cc indices
NCCS = __pyc_c_call__(int, "_CG_termios_const", int, 73)
VEOF = __pyc_c_call__(int, "_CG_termios_const", int, 74)
VEOL = __pyc_c_call__(int, "_CG_termios_const", int, 75)
VEOL2 = __pyc_c_call__(int, "_CG_termios_const", int, 76)
VERASE = __pyc_c_call__(int, "_CG_termios_const", int, 77)
VWERASE = __pyc_c_call__(int, "_CG_termios_const", int, 78)
VINTR = __pyc_c_call__(int, "_CG_termios_const", int, 79)
VKILL = __pyc_c_call__(int, "_CG_termios_const", int, 80)
VQUIT = __pyc_c_call__(int, "_CG_termios_const", int, 81)
VSTART = __pyc_c_call__(int, "_CG_termios_const", int, 82)
VSTOP = __pyc_c_call__(int, "_CG_termios_const", int, 83)
VSUSP = __pyc_c_call__(int, "_CG_termios_const", int, 84)
VREPRINT = __pyc_c_call__(int, "_CG_termios_const", int, 85)
VLNEXT = __pyc_c_call__(int, "_CG_termios_const", int, 86)
VDISCARD = __pyc_c_call__(int, "_CG_termios_const", int, 87)
VMIN = __pyc_c_call__(int, "_CG_termios_const", int, 88)
VTIME = __pyc_c_call__(int, "_CG_termios_const", int, 89)
# ioctl requests and modem bits
TIOCMGET = __pyc_c_call__(int, "_CG_termios_const", int, 90)
TIOCMSET = __pyc_c_call__(int, "_CG_termios_const", int, 91)
TIOCMBIS = __pyc_c_call__(int, "_CG_termios_const", int, 92)
TIOCMBIC = __pyc_c_call__(int, "_CG_termios_const", int, 93)
TIOCM_LE = __pyc_c_call__(int, "_CG_termios_const", int, 94)
TIOCM_DTR = __pyc_c_call__(int, "_CG_termios_const", int, 95)
TIOCM_RTS = __pyc_c_call__(int, "_CG_termios_const", int, 96)
TIOCM_ST = __pyc_c_call__(int, "_CG_termios_const", int, 97)
TIOCM_SR = __pyc_c_call__(int, "_CG_termios_const", int, 98)
TIOCM_CTS = __pyc_c_call__(int, "_CG_termios_const", int, 99)
TIOCM_CAR = __pyc_c_call__(int, "_CG_termios_const", int, 100)
TIOCM_CD = __pyc_c_call__(int, "_CG_termios_const", int, 101)
TIOCM_RNG = __pyc_c_call__(int, "_CG_termios_const", int, 102)
TIOCM_RI = __pyc_c_call__(int, "_CG_termios_const", int, 103)
TIOCM_DSR = __pyc_c_call__(int, "_CG_termios_const", int, 104)
FIONREAD = __pyc_c_call__(int, "_CG_termios_const", int, 105)
TIOCOUTQ = __pyc_c_call__(int, "_CG_termios_const", int, 106)
TIOCEXCL = __pyc_c_call__(int, "_CG_termios_const", int, 107)
TIOCNXCL = __pyc_c_call__(int, "_CG_termios_const", int, 108)
TIOCGWINSZ = __pyc_c_call__(int, "_CG_termios_const", int, 109)
TIOCSWINSZ = __pyc_c_call__(int, "_CG_termios_const", int, 110)


def tcgetattr(fd):
    __pyc_primitive__(__pyc_symbol__("__pyc_accommodation__"), "PYC_TERMIOS_CC_INT",
                      "termios.tcgetattr returns its cc list as ints, where CPython gives a 1-byte bytes for each "
                      "element other than VMIN and VTIME in non-canonical mode")
    return _tcgetattr_ints(fd)


# tcgetattr without the declaration, for library code that reads cc only at
# VMIN/VTIME after making the terminal non-canonical, or only passes it back
# to tcsetattr -- where the ints are what CPython gives, or are accepted as
# its bytes (pyc_lib/serial/serialposix.py). Not CPython API.
def _tcgetattr_ints(fd):
    t = __pyc_c_call__(bytes, "_CG_tc_get", int, _fd(fd))
    if __pyc_c_call__(int, "_CG_os_err") != 0:
        _fail()
    cc = []
    for i in range(NCCS):
        cc.append(__pyc_c_call__(int, "_CG_tc_cc", bytes, t, int, i))
    return [__pyc_c_call__(int, "_CG_tc_field", bytes, t, int, 0), __pyc_c_call__(int, "_CG_tc_field", bytes, t, int, 1),
            __pyc_c_call__(int, "_CG_tc_field", bytes, t, int, 2), __pyc_c_call__(int, "_CG_tc_field", bytes, t, int, 3),
            __pyc_c_call__(int, "_CG_tc_field", bytes, t, int, 4), __pyc_c_call__(int, "_CG_tc_field", bytes, t, int, 5),
            cc]


def _cc_value(c):
    if isinstance(c, int):
        return c & 0xff
    if isinstance(c, bytes):
        if len(c) == 1:
            return c[0]
    raise TypeError("tcsetattr: elements of attributes must be characters or integers")


def tcsetattr(fd, when, attributes):
    if len(attributes) != 7:
        raise TypeError("tcsetattr, arg 3: must be 7 element list")
    cc = attributes[6]
    if len(cc) != NCCS:
        raise TypeError("tcsetattr: attributes[6] must be " + str(NCCS) + " element list")
    values = []
    for c in cc:
        values.append(_cc_value(c))
    if __pyc_c_call__(int, "_CG_tc_set", int, _fd(fd), int, when, int, attributes[0], int, attributes[1],
                      int, attributes[2], int, attributes[3], int, attributes[4], int, attributes[5],
                      bytes, bytes(values)) < 0:
        _fail()


def tcsendbreak(fd, duration):
    if __pyc_c_call__(int, "_CG_tc_sendbreak", int, _fd(fd), int, duration) < 0:
        _fail()


def tcdrain(fd):
    if __pyc_c_call__(int, "_CG_tc_drain", int, _fd(fd)) < 0:
        _fail()


def tcflush(fd, queue):
    if __pyc_c_call__(int, "_CG_tc_flush", int, _fd(fd), int, queue) < 0:
        _fail()


def tcflow(fd, action):
    if __pyc_c_call__(int, "_CG_tc_flow", int, _fd(fd), int, action) < 0:
        _fail()


def tcgetwinsize(fd):
    p = __pyc_c_call__(int, "_CG_tc_getwinsize", int, _fd(fd))
    if p < 0:
        _fail()
    return (p & 0xffffffff, p >> 32)


def tcsetwinsize(fd, winsize):
    if len(winsize) != 2:
        raise TypeError("tcsetwinsize, arg 2: must be a two-item sequence")
    rows = winsize[0]
    cols = winsize[1]
    if rows < 0 or rows > 65535 or cols < 0 or cols > 65535:
        raise OverflowError("winsize value(s) out of range.")
    if __pyc_c_call__(int, "_CG_tc_setwinsize", int, _fd(fd), int, rows, int, cols) < 0:
        _fail()
