# pyc shim for the standard `fcntl` module: fcntl, ioctl and flock, with
# CPython's results and errors (OSError subclasses, as `os` raises them).
#
# The integer form of fcntl/ioctl passes the int and returns the call's
# result. The buffer form copies a bytes (or str, encoded) argument of at
# most 1024 bytes into a buffer the call may write, and returns a bytes of
# the argument's length, as CPython does. CPython's third form, a mutable
# buffer (bytearray) updated in place, raises NotImplementedError here
# rather than silently returning a copy. lockf and the struct-flock locking
# commands are not provided (a missing name).
#
# Constants come from C: their values are the platform's.

import os

F_DUPFD = __pyc_c_call__(int, "_CG_fcntl_const", int, 0)
F_DUPFD_CLOEXEC = __pyc_c_call__(int, "_CG_fcntl_const", int, 1)
F_GETFD = __pyc_c_call__(int, "_CG_fcntl_const", int, 2)
F_SETFD = __pyc_c_call__(int, "_CG_fcntl_const", int, 3)
F_GETFL = __pyc_c_call__(int, "_CG_fcntl_const", int, 4)
F_SETFL = __pyc_c_call__(int, "_CG_fcntl_const", int, 5)
FD_CLOEXEC = __pyc_c_call__(int, "_CG_fcntl_const", int, 6)
LOCK_SH = __pyc_c_call__(int, "_CG_fcntl_const", int, 7)
LOCK_EX = __pyc_c_call__(int, "_CG_fcntl_const", int, 8)
LOCK_NB = __pyc_c_call__(int, "_CG_fcntl_const", int, 9)
LOCK_UN = __pyc_c_call__(int, "_CG_fcntl_const", int, 10)


def _fd(fd):
    if isinstance(fd, int):
        return fd
    return fd.fileno()


def _buffer(arg, what):
    if isinstance(arg, bytearray):
        raise NotImplementedError("pyc: " + what + " with a mutable buffer argument is not implemented")
    if isinstance(arg, str):
        b = arg.encode()
    else:
        b = bytes(arg)
    if len(b) > 1024:
        raise ValueError(what + " string arg too long")
    return b


def _check(r):
    if r < 0:
        os._fail_fd(__pyc_c_call__(int, "_CG_os_err"))
    return r


def fcntl(fd, cmd, arg=0):
    if isinstance(arg, int):
        return _check(__pyc_c_call__(int, "_CG_fcntl_int", int, _fd(fd), int, cmd, int, arg))
    out = __pyc_c_call__(bytes, "_CG_fcntl_buf", int, _fd(fd), int, cmd, bytes, _buffer(arg, "fcntl"))
    if __pyc_c_call__(int, "_CG_os_err") != 0:
        os._fail_fd(__pyc_c_call__(int, "_CG_os_err"))
    return out


def ioctl(fd, request, arg=0):
    if isinstance(arg, int):
        return _check(__pyc_c_call__(int, "_CG_ioctl_int", int, _fd(fd), int, request, int, arg))
    out = __pyc_c_call__(bytes, "_CG_ioctl_buf", int, _fd(fd), int, request, bytes, _buffer(arg, "ioctl"))
    if __pyc_c_call__(int, "_CG_os_err") != 0:
        os._fail_fd(__pyc_c_call__(int, "_CG_os_err"))
    return out


def flock(fd, operation):
    _check(__pyc_c_call__(int, "_CG_flock", int, _fd(fd), int, operation))
