# pyc port of pyserial 3.5's serial/serialposix.py: the POSIX Serial class,
# termios and fcntl underneath. pySerial is (C) 2001-2020 Chris Liechti
# <cliechti@gmx.net>, BSD-3-Clause. serialutil.py lists what differs and
# why; in this file:
#
# - The platform is asked of C, not of sys.platform. Linux's extra ioctls
#   (TCGETS2/TCSETS2 for a non-standard baud rate, CMSPAR, IUCLC) are used
#   on Linux only, with pyserial's own values. macOS sets a non-standard rate
#   with IOSSIOSPEED, as pyserial does. RS485 and low-latency mode are not
#   provided.
# - tcgetattr's result is read by position, never unpacked or compared
#   whole, and getattr(termios, 'B<rate>') is a table of the rates termios
#   defines on both platforms.
# - An ioctl whose failure pyserial inspects by errno (DTR/RTS at open, where
#   EINVAL and ENOTTY are ignored) calls the runtime directly for the errno.
# - PosixPollSerial and VTIMESerial are not provided.

import os
import select
import struct
import termios
import fcntl

from serial.serialutil import SerialBase, SerialException, to_bytes, PortNotOpenError, \
    SerialTimeoutException, _Timeout, STOPBITS_ONE, STOPBITS_ONE_POINT_FIVE, STOPBITS_TWO, \
    PARITY_NONE, PARITY_EVEN, PARITY_ODD, PARITY_MARK, PARITY_SPACE

_LINUX = __pyc_c_call__(int, "_CG_os_const", int, 23) == 1
_EINVAL = __pyc_c_call__(int, "_CG_os_const", int, 21)
_ENOTTY = __pyc_c_call__(int, "_CG_os_const", int, 24)
_EAGAIN = __pyc_c_call__(int, "_CG_os_const", int, 16)
_EWOULDBLOCK = __pyc_c_call__(int, "_CG_os_const", int, 17)
_EINPROGRESS = __pyc_c_call__(int, "_CG_os_const", int, 18)
_EALREADY = __pyc_c_call__(int, "_CG_os_const", int, 19)

# getattr(termios, 'B<rate>') in pyserial: the rates termios defines on
# every POSIX platform pyc supports.
_TERMIOS_BAUD = {
    0: termios.B0, 50: termios.B50, 75: termios.B75, 110: termios.B110, 134: termios.B134,
    150: termios.B150, 200: termios.B200, 300: termios.B300, 600: termios.B600,
    1200: termios.B1200, 1800: termios.B1800, 2400: termios.B2400, 4800: termios.B4800,
    9600: termios.B9600, 19200: termios.B19200, 38400: termios.B38400, 57600: termios.B57600,
    115200: termios.B115200, 230400: termios.B230400,
}

if _LINUX:
    # pyserial's Linux constants
    CMSPAR = 0o10000000000
    _IUCLC = 0o1000
    _CBAUD = 0o10017
    TCGETS2 = 0x802C542A
    TCSETS2 = 0x402C542B
    BOTHER = 0o010000
    _BAUDRATE_CONSTANTS = {
        460800: 0o010004, 500000: 0o010005, 576000: 0o010006, 921600: 0o010007,
        1000000: 0o010010, 1152000: 0o010011, 1500000: 0o010012, 2000000: 0o010013,
        2500000: 0o010014, 3000000: 0o010015, 3500000: 0o010016, 4000000: 0o010017,
    }
    TIOCSBRK = 0x5427
    TIOCCBRK = 0x5428
else:
    CMSPAR = 0
    _IUCLC = 0
    _CBAUD = 0
    TCGETS2 = 0
    TCSETS2 = 0
    BOTHER = 0
    _BAUDRATE_CONSTANTS = {0: 0}
    TIOCSBRK = 0x2000747B  # _IO('t', 123), macOS and the BSDs
    TIOCCBRK = 0x2000747A
IOSSIOSPEED = 0x80045402  # _IOW('T', 2, speed_t), macOS

TIOCMGET = termios.TIOCMGET
TIOCMBIS = termios.TIOCMBIS
TIOCMBIC = termios.TIOCMBIC
TIOCMSET = termios.TIOCMSET
TIOCM_DTR = termios.TIOCM_DTR
TIOCM_RTS = termios.TIOCM_RTS
TIOCM_CTS = termios.TIOCM_CTS
TIOCM_CAR = termios.TIOCM_CAR
TIOCM_RNG = termios.TIOCM_RNG
TIOCM_DSR = termios.TIOCM_DSR
TIOCM_CD = termios.TIOCM_CD
TIOCM_RI = termios.TIOCM_RI
TIOCINQ = termios.FIONREAD
TIOCOUTQ = termios.TIOCOUTQ

TIOCM_zero_str = struct.pack('I', 0)
TIOCM_RTS_str = struct.pack('I', TIOCM_RTS)
TIOCM_DTR_str = struct.pack('I', TIOCM_DTR)


def _ioctl_errno(fd, request, arg):
    """fcntl.ioctl's buffer form, returning the errno (0 on success) where
    pyserial catches the OSError to read its errno."""
    __pyc_c_call__(bytes, "_CG_ioctl_buf", int, fd, int, request, bytes, arg)
    return __pyc_c_call__(int, "_CG_os_err")


def _blocking_errno(e):
    return e == _EAGAIN or e == _EALREADY or e == _EWOULDBLOCK or e == _EINPROGRESS


class Serial(SerialBase):
    """\
    Serial port class POSIX implementation. Serial port configuration is
    done with termios and fcntl. Runs on Linux and many other Un*x like
    systems.
    """

    def open(self):
        """\
        Open port with current settings. This may throw a SerialException
        if the port cannot be opened."""
        if self._port is None:
            raise SerialException("Port must be configured before it can be used.")
        if self.is_open:
            raise SerialException("Port is already open.")
        self.fd = -1
        fd = __pyc_c_call__(int, "_CG_os_open", str, self.portstr, int, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK,
                            int, 0o777)
        if fd < 0:
            e = __pyc_c_call__(int, "_CG_os_err")
            raise SerialException("[Errno " + str(e) + "] could not open port " + self._port + ": [Errno " + str(e) +
                                  "] " + os.strerror(e) + ": " + repr(self.portstr))
        self.fd = fd
        self.pipe_abort_read_r = -1
        self.pipe_abort_read_w = -1
        self.pipe_abort_write_r = -1
        self.pipe_abort_write_w = -1
        try:
            self._reconfigure_port(True)
            # DTR and RTS; EINVAL and ENOTTY (a device without modem lines)
            # are ignored, any other failure raises, as pyserial's do.
            if not self._dsrdtr:
                self._update_dtr_state_at_open()
            if not self._rtscts:
                self._update_rts_state_at_open()
            self._reset_input_buffer()
            self.pipe_abort_read_r, self.pipe_abort_read_w = os.pipe()
            self.pipe_abort_write_r, self.pipe_abort_write_w = os.pipe()
            fcntl.fcntl(self.pipe_abort_read_r, fcntl.F_SETFL, os.O_NONBLOCK)
            fcntl.fcntl(self.pipe_abort_write_r, fcntl.F_SETFL, os.O_NONBLOCK)
        except BaseException:
            __pyc_c_call__(int, "_CG_os_close", int, self.fd)
            self.fd = -1
            if self.pipe_abort_read_w >= 0:
                os.close(self.pipe_abort_read_w)
                self.pipe_abort_read_w = -1
            if self.pipe_abort_read_r >= 0:
                os.close(self.pipe_abort_read_r)
                self.pipe_abort_read_r = -1
            if self.pipe_abort_write_w >= 0:
                os.close(self.pipe_abort_write_w)
                self.pipe_abort_write_w = -1
            if self.pipe_abort_write_r >= 0:
                os.close(self.pipe_abort_write_r)
                self.pipe_abort_write_r = -1
            raise
        self.is_open = True

    def _modem_line_at_open(self, on, bits):
        e = _ioctl_errno(self.fd, TIOCMBIS if on else TIOCMBIC, bits)
        if e != 0 and e != _EINVAL and e != _ENOTTY:
            os._fail_fd(e)

    def _update_dtr_state_at_open(self):
        self._modem_line_at_open(self._dtr_state, TIOCM_DTR_str)

    def _update_rts_state_at_open(self):
        self._modem_line_at_open(self._rts_state, TIOCM_RTS_str)

    def _reconfigure_port(self, force_update=False):
        """Set communication parameters on opened port."""
        if self.fd < 0:
            raise SerialException("Can only operate on a valid file descriptor")

        # if exclusive lock is requested, create it before we modify anything else
        if self._exclusive is not None:
            if self._exclusive:
                if __pyc_c_call__(int, "_CG_flock", int, self.fd, int, fcntl.LOCK_EX | fcntl.LOCK_NB) < 0:
                    e = __pyc_c_call__(int, "_CG_os_err")
                    raise SerialException("[Errno " + str(e) + "] Could not exclusively lock port " + self._port +
                                          ": [Errno " + str(e) + "] " + os.strerror(e))
            else:
                fcntl.flock(self.fd, fcntl.LOCK_UN)

        custom_baud = -1

        vmin = 0                # timeout is done via select
        vtime = 0
        if self._inter_byte_timeout is not None:
            vmin = 1
            vtime = int(self._inter_byte_timeout * 10)
        try:
            # cc is read at VMIN/VTIME and passed back, so the int form is
            # exact here (pyc_lib/termios.py)
            orig_attr = termios._tcgetattr_ints(self.fd)
        except termios.error as msg:      # if a port is nonexistent but has a /dev file, it'll fail here
            raise SerialException("Could not configure port: " + str(msg))
        iflag = orig_attr[0]
        oflag = orig_attr[1]
        cflag = orig_attr[2]
        lflag = orig_attr[3]
        ispeed = orig_attr[4]
        ospeed = orig_attr[5]
        cc = orig_attr[6]
        orig_vmin = cc[termios.VMIN]
        orig_vtime = cc[termios.VTIME]
        # set up raw mode / no echo / binary
        cflag |= (termios.CLOCAL | termios.CREAD)
        lflag &= ~(termios.ICANON | termios.ECHO | termios.ECHOE |
                   termios.ECHOK | termios.ECHONL |
                   termios.ISIG | termios.IEXTEN)
        lflag &= ~termios.ECHOCTL
        lflag &= ~termios.ECHOKE

        oflag &= ~(termios.OPOST | termios.ONLCR | termios.OCRNL)
        iflag &= ~(termios.INLCR | termios.IGNCR | termios.ICRNL | termios.IGNBRK)
        iflag &= ~_IUCLC
        iflag &= ~termios.PARMRK

        # setup baud rate
        if self._baudrate in _TERMIOS_BAUD:
            ispeed = _TERMIOS_BAUD[self._baudrate]
            ospeed = ispeed
        elif _LINUX and self._baudrate in _BAUDRATE_CONSTANTS:
            ispeed = _BAUDRATE_CONSTANTS[self._baudrate]
            ospeed = ispeed
        else:
            if _LINUX:
                ispeed = BOTHER
            else:
                ispeed = termios.B38400
            ospeed = ispeed
            custom_baud = int(self._baudrate)
            if custom_baud < 0:
                raise ValueError('Invalid baud rate: ' + repr(self._baudrate))

        # setup char len
        cflag &= ~termios.CSIZE
        if self._bytesize == 8:
            cflag |= termios.CS8
        elif self._bytesize == 7:
            cflag |= termios.CS7
        elif self._bytesize == 6:
            cflag |= termios.CS6
        elif self._bytesize == 5:
            cflag |= termios.CS5
        else:
            raise ValueError('Invalid char len: ' + repr(self._bytesize))
        # setup stop bits
        if self._stopbits == STOPBITS_ONE:
            cflag &= ~(termios.CSTOPB)
        elif self._stopbits == STOPBITS_ONE_POINT_FIVE:
            cflag |= (termios.CSTOPB)  # XXX same as TWO.. there is no POSIX support for 1.5
        elif self._stopbits == STOPBITS_TWO:
            cflag |= (termios.CSTOPB)
        else:
            raise ValueError('Invalid stop bit specification: ' + repr(self._stopbits))
        # setup parity
        iflag &= ~(termios.INPCK | termios.ISTRIP)
        if self._parity == PARITY_NONE:
            cflag &= ~(termios.PARENB | termios.PARODD | CMSPAR)
        elif self._parity == PARITY_EVEN:
            cflag &= ~(termios.PARODD | CMSPAR)
            cflag |= (termios.PARENB)
        elif self._parity == PARITY_ODD:
            cflag &= ~CMSPAR
            cflag |= (termios.PARENB | termios.PARODD)
        elif self._parity == PARITY_MARK and CMSPAR:
            cflag |= (termios.PARENB | CMSPAR | termios.PARODD)
        elif self._parity == PARITY_SPACE and CMSPAR:
            cflag |= (termios.PARENB | CMSPAR)
            cflag &= ~(termios.PARODD)
        else:
            raise ValueError('Invalid parity: ' + repr(self._parity))
        # setup flow control
        if self._xonxoff:
            iflag |= (termios.IXON | termios.IXOFF)
        else:
            iflag &= ~(termios.IXON | termios.IXOFF | termios.IXANY)
        if self._rtscts:
            cflag |= (termios.CRTSCTS)
        else:
            cflag &= ~(termios.CRTSCTS)

        if vmin < 0 or vmin > 255:
            raise ValueError('Invalid vmin: ' + repr(vmin))
        cc[termios.VMIN] = vmin
        if vtime < 0 or vtime > 255:
            raise ValueError('Invalid vtime: ' + repr(vtime))
        cc[termios.VTIME] = vtime
        # activate settings: pyserial compares the whole new list with the
        # original; only these fields and VMIN/VTIME of cc can differ.
        if (force_update or iflag != orig_attr[0] or oflag != orig_attr[1] or cflag != orig_attr[2] or
                lflag != orig_attr[3] or ispeed != orig_attr[4] or ospeed != orig_attr[5] or
                vmin != orig_vmin or vtime != orig_vtime):
            termios.tcsetattr(self.fd, termios.TCSANOW, [iflag, oflag, cflag, lflag, ispeed, ospeed, cc])

        # apply custom baud rate, if any
        if custom_baud >= 0:
            self._set_special_baudrate(custom_baud)

    def _set_special_baudrate(self, baudrate):
        if _LINUX:
            # struct termios2: c_cflag is int 2, c_ispeed and c_ospeed ints 9
            # and 10, as pyserial indexes its array('i'); 64 ints of room
            try:
                buf = fcntl.ioctl(self.fd, TCGETS2, b"\x00" * 256)
                cflag = (struct.unpack_from('i', buf, 8)[0] & ~_CBAUD) | BOTHER
                buf = buf[:8] + struct.pack('i', cflag) + buf[12:36] + struct.pack('i', baudrate) + \
                    struct.pack('i', baudrate) + buf[44:]
                fcntl.ioctl(self.fd, TCSETS2, buf)
            except OSError as e:
                raise ValueError('Failed to set custom baud rate (' + str(baudrate) + '): ' + str(e))
        else:
            fcntl.ioctl(self.fd, IOSSIOSPEED, struct.pack('i', baudrate))

    def close(self):
        """Close port"""
        if self.is_open:
            if self.fd >= 0:
                os.close(self.fd)
                self.fd = -1
                os.close(self.pipe_abort_read_w)
                os.close(self.pipe_abort_read_r)
                os.close(self.pipe_abort_write_w)
                os.close(self.pipe_abort_write_r)
                self.pipe_abort_read_r = -1
                self.pipe_abort_read_w = -1
                self.pipe_abort_write_r = -1
                self.pipe_abort_write_w = -1
            self.is_open = False

    #  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -

    @property
    def in_waiting(self):
        """Return the number of bytes currently in the input buffer."""
        s = fcntl.ioctl(self.fd, TIOCINQ, TIOCM_zero_str)
        return struct.unpack('I', s)[0]

    def _select_read(self, timeout):
        if timeout.is_infinite:
            return select.select([self.fd, self.pipe_abort_read_r], [], [], None)[0]
        return select.select([self.fd, self.pipe_abort_read_r], [], [], timeout.time_left())[0]

    def read(self, size=1):
        """\
        Read size bytes from the serial port. If a timeout is set it may
        return less characters as requested. With no timeout it will block
        until the requested number of bytes is read.
        """
        if not self.is_open:
            raise PortNotOpenError()
        read = b""
        timeout = _Timeout(self._timeout)
        while len(read) < size:
            ready = self._select_read(timeout)
            if self.pipe_abort_read_r in ready:
                os.read(self.pipe_abort_read_r, 1000)
                break
            # If select was used with a timeout, and the timeout occurs, it
            # returns with empty lists -> thus abort read operation.
            # For timeout == 0 (non-blocking operation) also abort when
            # there is nothing to read.
            if not ready:
                break   # timeout
            buf = __pyc_c_call__(bytes, "_CG_os_read", int, self.fd, int, size - len(read))
            e = __pyc_c_call__(int, "_CG_os_err")
            if e != 0:
                # BlockingIOErrors are ignored, as pyserial's are (PEP 475:
                # an interrupted read is already retried)
                if not _blocking_errno(e):
                    raise SerialException('read failed: [Errno ' + str(e) + '] ' + os.strerror(e))
            else:
                # read should always return some data as select reported it was
                # ready to read when we get to this point.
                if not buf:
                    # Disconnected devices, at least on Linux, show the
                    # behavior that they are always ready to read immediately
                    # but reading returns nothing.
                    raise SerialException(
                        'device reports readiness to read but returned no data '
                        '(device disconnected or multiple access on port?)')
                read = read + buf
            if timeout.expired():
                break
        return read

    def cancel_read(self):
        if self.is_open:
            os.write(self.pipe_abort_read_w, b"x")

    def cancel_write(self):
        if self.is_open:
            os.write(self.pipe_abort_write_w, b"x")

    def write(self, data):
        """Output the given byte string over the serial port."""
        if not self.is_open:
            raise PortNotOpenError()
        d = to_bytes(data)
        length = len(d)
        tx_len = length
        timeout = _Timeout(self._write_timeout)
        while tx_len > 0:
            n = __pyc_c_call__(int, "_CG_os_write", int, self.fd, bytes, d)
            if n < 0:
                e = __pyc_c_call__(int, "_CG_os_err")
                if not _blocking_errno(e):
                    raise SerialException('write failed: [Errno ' + str(e) + '] ' + os.strerror(e))
            else:
                if timeout.is_non_blocking:
                    # Zero timeout indicates non-blocking - simply return the
                    # number of bytes of data actually written
                    return n
                elif not timeout.is_infinite:
                    # when timeout is set, use select to wait for being ready
                    # with the time left as timeout
                    if timeout.expired():
                        raise SerialTimeoutException('Write timeout')
                    r = select.select([self.pipe_abort_write_r], [self.fd], [], timeout.time_left())
                    if r[0]:
                        os.read(self.pipe_abort_write_r, 1000)
                        break
                    if not r[1]:
                        raise SerialTimeoutException('Write timeout')
                else:
                    # wait for write operation
                    r = select.select([self.pipe_abort_write_r], [self.fd], [], None)
                    if r[0]:
                        os.read(self.pipe_abort_write_r, 1)
                        break
                    if not r[1]:
                        raise SerialException('write failed (select)')
                d = d[n:]
                tx_len -= n
            if not timeout.is_non_blocking and timeout.expired():
                raise SerialTimeoutException('Write timeout')
        return length - len(d)

    def flush(self):
        """\
        Flush of file like objects. In this case, wait until all data
        is written.
        """
        if not self.is_open:
            raise PortNotOpenError()
        termios.tcdrain(self.fd)

    def _reset_input_buffer(self):
        """Clear input buffer, discarding all that is in the buffer."""
        termios.tcflush(self.fd, termios.TCIFLUSH)

    def reset_input_buffer(self):
        """Clear input buffer, discarding all that is in the buffer."""
        if not self.is_open:
            raise PortNotOpenError()
        self._reset_input_buffer()

    def reset_output_buffer(self):
        """\
        Clear output buffer, aborting the current output and discarding all
        that is in the buffer.
        """
        if not self.is_open:
            raise PortNotOpenError()
        termios.tcflush(self.fd, termios.TCOFLUSH)

    def send_break(self, duration=0.25):
        """\
        Send break condition. Timed, returns to idle state after given
        duration.
        """
        if not self.is_open:
            raise PortNotOpenError()
        termios.tcsendbreak(self.fd, int(duration / 0.25))

    def _update_break_state(self):
        if self._break_state:
            fcntl.ioctl(self.fd, TIOCSBRK)
        else:
            fcntl.ioctl(self.fd, TIOCCBRK)

    def _update_rts_state(self):
        """Set terminal status line: Request To Send"""
        if self._rts_state:
            fcntl.ioctl(self.fd, TIOCMBIS, TIOCM_RTS_str)
        else:
            fcntl.ioctl(self.fd, TIOCMBIC, TIOCM_RTS_str)

    def _update_dtr_state(self):
        """Set terminal status line: Data Terminal Ready"""
        if self._dtr_state:
            fcntl.ioctl(self.fd, TIOCMBIS, TIOCM_DTR_str)
        else:
            fcntl.ioctl(self.fd, TIOCMBIC, TIOCM_DTR_str)

    def _modem_bits(self):
        if not self.is_open:
            raise PortNotOpenError()
        s = fcntl.ioctl(self.fd, TIOCMGET, TIOCM_zero_str)
        return struct.unpack('I', s)[0]

    @property
    def cts(self):
        """Read terminal status line: Clear To Send"""
        return self._modem_bits() & TIOCM_CTS != 0

    @property
    def dsr(self):
        """Read terminal status line: Data Set Ready"""
        return self._modem_bits() & TIOCM_DSR != 0

    @property
    def ri(self):
        """Read terminal status line: Ring Indicator"""
        return self._modem_bits() & TIOCM_RI != 0

    @property
    def cd(self):
        """Read terminal status line: Carrier Detect"""
        return self._modem_bits() & TIOCM_CD != 0

    @property
    def out_waiting(self):
        """Return the number of bytes currently in the output buffer."""
        s = fcntl.ioctl(self.fd, TIOCOUTQ, TIOCM_zero_str)
        return struct.unpack('I', s)[0]

    def fileno(self):
        """\
        For easier use of the serial port instance with select.
        WARNING: this function is not portable to different platforms!
        """
        if not self.is_open:
            raise PortNotOpenError()
        return self.fd

    def set_input_flow_control(self, enable=True):
        if not self.is_open:
            raise PortNotOpenError()
        if enable:
            termios.tcflow(self.fd, termios.TCION)
        else:
            termios.tcflow(self.fd, termios.TCIOFF)

    def set_output_flow_control(self, enable=True):
        if not self.is_open:
            raise PortNotOpenError()
        if enable:
            termios.tcflow(self.fd, termios.TCOON)
        else:
            termios.tcflow(self.fd, termios.TCOOFF)
