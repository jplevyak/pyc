# pyc port of pyserial 3.5's serial/serialutil.py: constants, exceptions and
# SerialBase, the settings half of a port. pySerial is (C) 2001-2020 Chris
# Liechti <cliechti@gmx.net>, BSD-3-Clause; this follows its code, and every
# difference is one pyc's typing needs, listed here:
#
# - A setting is never stored as None and then as a number in one object.
#   pyserial's __init__ first writes `self._timeout = None` and then the
#   argument; this writes the argument once. A port that is GIVEN None keeps
#   None, one given 1 keeps 1, and a program that changes a timeout between
#   None and a number on one port is refused at compile time ({None, number}
#   has no representation) instead of running differently.
# - The timeout helper is private (_Timeout): its time_left() returns a float
#   and is not called for an infinite timeout, where pyserial's returns None.
# - get_settings/apply_settings (a dict of mixed value types), rs485_mode,
#   iread_until, readinto and __repr__ are not provided: a missing name, a
#   compile error, never a wrong answer. Unknown keyword arguments are a
#   compile error rather than pyserial's ValueError.
# - SerialBase does not derive from io.RawIOBase. The methods a program uses
#   from it (readline, readlines, iteration, readable/writable/seekable) are
#   written out with io's semantics.
# - An exception's errno is part of its message ("[Errno 2] could not open
#   port ..."), as CPython prints it; pyc's OSError has no errno attribute.

import time

XON = b"\x11"
XOFF = b"\x13"
CR = b"\r"
LF = b"\n"

PARITY_NONE = 'N'
PARITY_EVEN = 'E'
PARITY_ODD = 'O'
PARITY_MARK = 'M'
PARITY_SPACE = 'S'
STOPBITS_ONE = 1
STOPBITS_ONE_POINT_FIVE = 1.5
STOPBITS_TWO = 2
FIVEBITS = 5
SIXBITS = 6
SEVENBITS = 7
EIGHTBITS = 8

PARITY_NAMES = {
    PARITY_NONE: 'None',
    PARITY_EVEN: 'Even',
    PARITY_ODD: 'Odd',
    PARITY_MARK: 'Mark',
    PARITY_SPACE: 'Space',
}


def to_bytes(seq):
    """convert a sequence to a bytes type"""
    if isinstance(seq, bytes):
        return seq
    if isinstance(seq, str):
        raise TypeError('unicode strings are not supported, please encode to bytes: ' + repr(seq))
    return bytes(seq)


class SerialException(IOError):
    """Base class for serial port related exceptions."""


class SerialTimeoutException(SerialException):
    """Write timeouts give an exception"""


class PortNotOpenError(SerialException):
    """Port is not open"""
    def __init__(self):
        SerialException.__init__(self, 'Attempting to use a port that is not open')


class _Timeout:
    """pyserial's Timeout over time.monotonic(). time_left() is only asked of
    a finite timeout (see the module comment)."""

    def __init__(self, duration):
        self.is_infinite = duration is None
        self.is_non_blocking = False
        self.duration = 0.0
        self.target_time = 0.0
        if duration is not None:
            self.is_non_blocking = duration == 0
            self.duration = float(duration)
            self.target_time = time.monotonic() + self.duration

    def expired(self):
        """Return a boolean, telling if the timeout has expired"""
        return not self.is_infinite and self.time_left() <= 0

    def time_left(self):
        """Return how many seconds are left until the timeout expires"""
        if self.is_non_blocking:
            return 0.0
        delta = self.target_time - time.monotonic()
        if delta > self.duration:
            # clock jumped, recalculate
            self.target_time = time.monotonic() + self.duration
            return self.duration
        return max(0.0, delta)

    def restart(self, duration):
        self.duration = float(duration)
        self.target_time = time.monotonic() + self.duration


def _class_name(x):
    # str(type(x)) for the values a port might wrongly be given; pyc has no
    # type() builtin
    if isinstance(x, bool):
        return "<class 'bool'>"
    if isinstance(x, int):
        return "<class 'int'>"
    if isinstance(x, float):
        return "<class 'float'>"
    if isinstance(x, bytes):
        return "<class 'bytes'>"
    return "<class 'object'>"


def _check_port(port):
    # pyserial's `port is not None and not isinstance(port, basestring)`,
    # asked as the types a port might wrongly be: each of these folds at
    # compile time for a {None, str} port, where isinstance(port, str) would
    # have to be decided at run time on a str, which carries no class tag
    # (issues/173). Any other non-str is refused at compile time, at the C
    # call that opens the port.
    if isinstance(port, bool) or isinstance(port, int) or isinstance(port, float) or isinstance(port, bytes):
        raise ValueError('"port" must be None or a string, not ' + _class_name(port))


def _check_timeout(timeout):
    if timeout is not None:
        if timeout < 0:
            raise ValueError("Not a valid timeout: " + repr(timeout))


class SerialBase:
    """\
    Serial port base class. Provides __init__ function and properties to
    get/set port settings.
    """

    BAUDRATES = (50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800,
                 9600, 19200, 38400, 57600, 115200, 230400, 460800, 500000,
                 576000, 921600, 1000000, 1152000, 1500000, 2000000, 2500000,
                 3000000, 3500000, 4000000)
    BYTESIZES = (FIVEBITS, SIXBITS, SEVENBITS, EIGHTBITS)
    PARITIES = (PARITY_NONE, PARITY_EVEN, PARITY_ODD, PARITY_MARK, PARITY_SPACE)
    STOPBITS = (STOPBITS_ONE, STOPBITS_ONE_POINT_FIVE, STOPBITS_TWO)

    def __init__(self,
                 port=None,
                 baudrate=9600,
                 bytesize=EIGHTBITS,
                 parity=PARITY_NONE,
                 stopbits=STOPBITS_ONE,
                 timeout=None,
                 xonxoff=False,
                 rtscts=False,
                 write_timeout=None,
                 dsrdtr=False,
                 inter_byte_timeout=None,
                 exclusive=None):
        """\
        Initialize comm port object. If a "port" is given, then the port will be
        opened immediately. Otherwise a Serial port object in closed state
        is returned.
        """
        self.is_open = False
        self.fd = -1
        self._rts_state = True
        self._dtr_state = True
        self._break_state = False
        # pyserial assigns these through the property setters; the setters'
        # checks are made here directly, and each field is written once.
        _check_port(port)
        self.portstr = port
        self._port = port
        self.name = port
        if baudrate < 0:
            raise ValueError("Not a valid baudrate: " + repr(baudrate))
        self._baudrate = int(baudrate)
        if bytesize not in self.BYTESIZES:
            raise ValueError("Not a valid byte size: " + repr(bytesize))
        self._bytesize = bytesize
        if parity not in self.PARITIES:
            raise ValueError("Not a valid parity: " + repr(parity))
        self._parity = parity
        if stopbits not in self.STOPBITS:
            raise ValueError("Not a valid stop bit size: " + repr(stopbits))
        self._stopbits = stopbits
        _check_timeout(timeout)
        self._timeout = timeout
        _check_timeout(write_timeout)
        self._write_timeout = write_timeout
        self._xonxoff = xonxoff
        self._rtscts = rtscts
        if dsrdtr is None:
            self._dsrdtr = rtscts
        else:
            self._dsrdtr = dsrdtr
        _check_timeout(inter_byte_timeout)
        self._inter_byte_timeout = inter_byte_timeout
        self._exclusive = exclusive

        if port is not None:
            self.open()

    #  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -

    @property
    def port(self):
        return self._port

    @port.setter
    def port(self, port):
        _check_port(port)
        was_open = self.is_open
        if was_open:
            self.close()
        self.portstr = port
        self._port = port
        self.name = self.portstr
        if was_open:
            self.open()

    @property
    def baudrate(self):
        return self._baudrate

    @baudrate.setter
    def baudrate(self, baudrate):
        b = int(baudrate)
        if b < 0:
            raise ValueError("Not a valid baudrate: " + repr(baudrate))
        self._baudrate = b
        if self.is_open:
            self._reconfigure_port()

    @property
    def bytesize(self):
        return self._bytesize

    @bytesize.setter
    def bytesize(self, bytesize):
        if bytesize not in self.BYTESIZES:
            raise ValueError("Not a valid byte size: " + repr(bytesize))
        self._bytesize = bytesize
        if self.is_open:
            self._reconfigure_port()

    @property
    def exclusive(self):
        return self._exclusive

    @exclusive.setter
    def exclusive(self, exclusive):
        self._exclusive = exclusive
        if self.is_open:
            self._reconfigure_port()

    @property
    def parity(self):
        return self._parity

    @parity.setter
    def parity(self, parity):
        if parity not in self.PARITIES:
            raise ValueError("Not a valid parity: " + repr(parity))
        self._parity = parity
        if self.is_open:
            self._reconfigure_port()

    @property
    def stopbits(self):
        return self._stopbits

    @stopbits.setter
    def stopbits(self, stopbits):
        if stopbits not in self.STOPBITS:
            raise ValueError("Not a valid stop bit size: " + repr(stopbits))
        self._stopbits = stopbits
        if self.is_open:
            self._reconfigure_port()

    @property
    def timeout(self):
        return self._timeout

    @timeout.setter
    def timeout(self, timeout):
        _check_timeout(timeout)
        self._timeout = timeout
        if self.is_open:
            self._reconfigure_port()

    @property
    def write_timeout(self):
        return self._write_timeout

    @write_timeout.setter
    def write_timeout(self, timeout):
        _check_timeout(timeout)
        self._write_timeout = timeout
        if self.is_open:
            self._reconfigure_port()

    @property
    def inter_byte_timeout(self):
        return self._inter_byte_timeout

    @inter_byte_timeout.setter
    def inter_byte_timeout(self, ic_timeout):
        _check_timeout(ic_timeout)
        self._inter_byte_timeout = ic_timeout
        if self.is_open:
            self._reconfigure_port()

    @property
    def xonxoff(self):
        return self._xonxoff

    @xonxoff.setter
    def xonxoff(self, xonxoff):
        self._xonxoff = xonxoff
        if self.is_open:
            self._reconfigure_port()

    @property
    def rtscts(self):
        return self._rtscts

    @rtscts.setter
    def rtscts(self, rtscts):
        self._rtscts = rtscts
        if self.is_open:
            self._reconfigure_port()

    @property
    def dsrdtr(self):
        return self._dsrdtr

    @dsrdtr.setter
    def dsrdtr(self, dsrdtr):
        if dsrdtr is None:
            self._dsrdtr = self._rtscts
        else:
            self._dsrdtr = dsrdtr
        if self.is_open:
            self._reconfigure_port()

    @property
    def rts(self):
        return self._rts_state

    @rts.setter
    def rts(self, value):
        self._rts_state = value
        if self.is_open:
            self._update_rts_state()

    @property
    def dtr(self):
        return self._dtr_state

    @dtr.setter
    def dtr(self, value):
        self._dtr_state = value
        if self.is_open:
            self._update_dtr_state()

    @property
    def break_condition(self):
        return self._break_state

    @break_condition.setter
    def break_condition(self, value):
        self._break_state = value
        if self.is_open:
            self._update_break_state()

    #  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
    # io.RawIOBase's interface

    def readable(self):
        return True

    def writable(self):
        return True

    def seekable(self):
        return False

    def readline(self, size=-1):
        """io.IOBase.readline: read(1) until a newline, size or an empty read"""
        line = b""
        while size < 0 or len(line) < size:
            c = self.read(1)
            if not c:
                break
            line = line + c
            if c == b"\n":
                break
        return line

    def readlines(self, hint=-1):
        """io.IOBase.readlines"""
        lines = []
        total = 0
        while True:
            line = self.readline()
            if not line:
                break
            lines.append(line)
            total += len(line)
            if hint > 0 and total >= hint:
                break
        return lines

    def __iter__(self):
        while True:
            line = self.readline()
            if not line:
                break
            yield line

    #  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
    # context manager

    def __enter__(self):
        if self._port is not None and not self.is_open:
            self.open()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()

    #  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -

    def send_break(self, duration=0.25):
        if not self.is_open:
            raise PortNotOpenError()
        self.break_condition = True
        time.sleep(duration)
        self.break_condition = False

    #  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
    # backwards compatibility / deprecated functions

    def flushInput(self):
        self.reset_input_buffer()

    def flushOutput(self):
        self.reset_output_buffer()

    def inWaiting(self):
        return self.in_waiting

    def sendBreak(self, duration=0.25):
        self.send_break(duration)

    def setRTS(self, value=1):
        self.rts = value

    def setDTR(self, value=1):
        self.dtr = value

    def getCTS(self):
        return self.cts

    def getDSR(self):
        return self.dsr

    def getRI(self):
        return self.ri

    def getCD(self):
        return self.cd

    def setPort(self, port):
        self.port = port

    @property
    def writeTimeout(self):
        return self.write_timeout

    @writeTimeout.setter
    def writeTimeout(self, timeout):
        self.write_timeout = timeout

    @property
    def interCharTimeout(self):
        return self.inter_byte_timeout

    @interCharTimeout.setter
    def interCharTimeout(self, interCharTimeout):
        self.inter_byte_timeout = interCharTimeout

    def isOpen(self):
        return self.is_open

    #  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
    # additional functionality

    def read_all(self):
        return self.read(self.in_waiting)

    def read_until(self, expected=LF, size=None):
        lenterm = len(expected)
        line = b""
        timeout = _Timeout(self._timeout)
        while True:
            c = self.read(1)
            if c:
                line = line + c
                if line[-lenterm:] == expected:
                    break
                if size is not None and len(line) >= size:
                    break
            else:
                break
            if timeout.expired():
                break
        return line
