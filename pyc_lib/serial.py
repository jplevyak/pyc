# serial: the pyserial 3.5 API surface, for type-checking programs that use
# it. pyc cannot open a serial port (configuring one needs termios), so,
# per issues/041 ("real or raise"), opening a port raises SerialException
# and I/O on an unopened port raises PortNotOpenError -- exactly what
# pyserial does for a port it cannot open or has not opened. Nothing
# returns a plausible made-up value.
#
# The previous stub took only (port, baudrate), so msp_ss's
# `serial.Serial(port, 9600, 8, 'E', 1, timeout, 0, 0)` -- valid pyserial
# -- resolved to nothing, and its `read` returned "" where pyserial
# returns bytes.

PARITY_NONE = 'N'
PARITY_EVEN = 'E'
PARITY_ODD = 'O'
PARITY_MARK = 'M'
PARITY_SPACE = 'S'
STOPBITS_ONE = 1
STOPBITS_TWO = 2
FIVEBITS = 5
SIXBITS = 6
SEVENBITS = 7
EIGHTBITS = 8


class SerialException(IOError):
    pass


class PortNotOpenError(SerialException):
    def __init__(self):
        SerialException.__init__(self, "Attempting to use a port that is not open")


class Serial:
    def __init__(self, port=None, baudrate=9600, bytesize=8, parity='N', stopbits=1, timeout=None,
                 xonxoff=False, rtscts=False):
        self.port = port
        self.baudrate = baudrate
        self.bytesize = bytesize
        self.parity = parity
        self.stopbits = stopbits
        self.timeout = timeout
        self.xonxoff = xonxoff
        self.rtscts = rtscts
        self.is_open = False
        # pyserial opens the port here when one is given.
        if port is not None:
            self.open()

    def open(self):
        raise SerialException("could not open port " + str(self.port) +
                              ": pyc does not support serial ports (issues/041)")

    def read(self, size=1):
        raise PortNotOpenError()

    def write(self, data):
        raise PortNotOpenError()

    def inWaiting(self):
        raise PortNotOpenError()

    def flushInput(self):
        raise PortNotOpenError()

    def flushOutput(self):
        raise PortNotOpenError()

    def reset_input_buffer(self):
        raise PortNotOpenError()

    def reset_output_buffer(self):
        raise PortNotOpenError()

    def setDTR(self, value=True):
        raise PortNotOpenError()

    def setRTS(self, value=True):
        raise PortNotOpenError()

    def close(self):
        self.is_open = False
