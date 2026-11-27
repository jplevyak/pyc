# pyserial over a pseudo-terminal: the port is the pty's slave, opened by
# name, and the test plays the device on the master side. Reads with and
# without timeouts, readline/readlines/read_until/iteration, settings
# changed on an open port, cancel_read, the context manager, exclusive
# access, and the errors. .exec.check is pyserial 3.5 under CPython; the
# CPython cross-check is .python.expect_fail because pyserial is not part
# of CPython, and an interpreter without it cannot import this file.
import os
import serial
import termios


def err(f):
    try:
        f()
        print("no error")
    except serial.SerialTimeoutException as e:
        print("SerialTimeoutException", e)
    except serial.PortNotOpenError as e:
        print("PortNotOpenError", e)
    except serial.SerialException as e:
        print("SerialException", e)
    except OSError as e:
        print("OSError", e)
    except ValueError as e:
        print("ValueError", e)


# What the port writes reaches the master asynchronously (the kernel pushes
# the tty flip buffer from a work queue), so one os.read returns only what
# has arrived: two port writes can come back as two reads under load. Read
# the master until the expected count is in.
def read_master(n):
    got = b""
    while len(got) < n:
        got += os.read(m, n - len(got))
    return got


m, s = os.openpty()
name = os.ttyname(s)

ser = serial.Serial(name, 115200, timeout=0.3)
print(ser.is_open, ser.name == name, ser.port == name, ser.baudrate, ser.bytesize, ser.parity, ser.stopbits,
      ser.timeout, ser.xonxoff, ser.rtscts, ser.dsrdtr)
print(ser.readable(), ser.writable(), ser.seekable(), ser.isOpen(), ser.fileno() >= 0)

print(ser.write(b"hello"), ser.write([1, 2, 3]))
print(read_master(8))

os.write(m, b"abc\n")
print(ser.read(2), ser.in_waiting, ser.inWaiting(), ser.readline())
os.write(m, b"line1\nline2\npartial")
print(ser.readlines())
os.write(m, b"one\ntwo\n")
for line in ser:
    print("iter", line)
os.write(m, b"xyz")
print(ser.read(10))
os.write(m, b"data END tail")
print(ser.read_until(b"END"), ser.read_all())
os.write(m, b"12345678")
print(ser.read_until(b"Z", 4), ser.read(100))

ser.timeout = 0.0
print(ser.read(5), ser.timeout)
ser.timeout = 0.3

# A pty refuses some settings (EINVAL from tcsetattr, which pyserial lets
# through as termios.error) and keeps others; the kernel decides, so these
# show the port asks it for exactly what pyserial asks.
def set_bytesize(v):
    ser.bytesize = v


def set_parity(v):
    ser.parity = v


def report(f, v):
    try:
        f(v)
        print("ok", v)
    except termios.error as e:
        print("termios.error", v, e)


report(set_bytesize, serial.SEVENBITS)
report(set_bytesize, serial.EIGHTBITS)
report(set_parity, serial.PARITY_EVEN)
report(set_parity, serial.PARITY_ODD)
report(set_parity, serial.PARITY_MARK)
report(set_parity, serial.PARITY_NONE)
ser.baudrate = 9600
ser.stopbits = serial.STOPBITS_TWO
ser.xonxoff = True
ser.rtscts = True
print(ser.baudrate, ser.bytesize, ser.parity, ser.stopbits, ser.xonxoff, ser.rtscts)
ser.xonxoff = False
ser.rtscts = False
ser.stopbits = serial.STOPBITS_ONE
os.write(m, b"after")
print(ser.read(5))
ser.baudrate = 250000
print(ser.baudrate)
ser.baudrate = 921600
print(ser.baudrate)

err(lambda: ser.setDTR(False))
err(lambda: ser.cts)
print(ser.out_waiting)
ser.reset_input_buffer()
ser.reset_output_buffer()
ser.flushInput()
ser.flushOutput()
ser.flush()
ser.send_break(0)
ser.set_output_flow_control(True)

ser.cancel_read()
print(ser.read(5))

err(lambda: serial.Serial(name, 9600, exclusive=True))
ser.close()
ser.close()
print(ser.is_open)
err(lambda: ser.read(1))
err(lambda: ser.write(b"x"))
err(lambda: ser.flush())
err(lambda: ser.fileno())

with serial.Serial(name, 9600, timeout=0.3) as p:
    p.write(b"ctx")
    print(p.is_open, read_master(3))
print(p.is_open)

q = serial.Serial(name, 19200, 8, 'E', 1, 0.3, 0, 0)
print(q.baudrate, q.bytesize, q.parity, q.stopbits, q.timeout, q.xonxoff, q.rtscts)
q.close()

r = serial.Serial(timeout=0.3)
print(r.is_open, r.port)
r.port = name
r.open()
print(r.is_open)
err(lambda: r.open())
r.close()

err(lambda: serial.Serial("/dev/no_such_pyc_port"))
err(lambda: serial.Serial(None, -1))
err(lambda: serial.Serial(None, 9600, 9))
err(lambda: serial.Serial(None, 9600, 8, 'X'))
err(lambda: serial.Serial(None, 9600, 8, 'N', 3))
err(lambda: serial.Serial(None, 9600, 8, 'N', 1, -1.0))
err(lambda: serial.Serial(timeout=0.3).open())
os.close(m)
os.close(s)
