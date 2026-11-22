# pyc port of pyserial 3.5's serial package (POSIX). pySerial is (C)
# 2001-2020 Chris Liechti <cliechti@gmx.net>, BSD-3-Clause.
#
# `serial.Serial` is serialposix's. serial_for_url, the URL handlers
# (loop://, socket://, rfc2217://), serial.tools (list_ports, miniterm),
# rs485, PosixPollSerial and VTIMESerial are not provided: a missing name,
# a compile error, never a wrong answer. serialutil.py lists what differs
# from pyserial and why.
#
# This package replaces a stub (issues/041) whose open() always raised.

from serial.serialutil import XON, XOFF, CR, LF, PARITY_NONE, PARITY_EVEN, PARITY_ODD, PARITY_MARK, \
    PARITY_SPACE, STOPBITS_ONE, STOPBITS_ONE_POINT_FIVE, STOPBITS_TWO, FIVEBITS, SIXBITS, SEVENBITS, \
    EIGHTBITS, PARITY_NAMES, SerialException, SerialTimeoutException, PortNotOpenError, SerialBase, \
    to_bytes
from serial.serialposix import Serial

__version__ = '3.5'
VERSION = __version__
