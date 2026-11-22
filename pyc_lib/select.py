# pyc shim for the standard `select` module: select.select, with CPython's
# results and errors.
#
# Implemented over poll(2) (`_CG_poll_*`, pyc_runtime.c), reporting what
# select(2) would: a descriptor that has hung up or failed is readable. It
# keeps select's own limits, as CPython does: a descriptor of FD_SETSIZE or
# more is a ValueError. The timeout is in seconds, rounded up to the
# millisecond (never returning early), and None waits indefinitely, even
# with all three lists empty. An interrupted wait resumes with what is left
# of the timeout (PEP 475).
#
# The previous shim polled the read list every 50 ms in a loop, ignored the
# timeout's value and the write and exceptional lists, and returned at once
# when the read list was empty.
#
# select.poll, epoll and kqueue are not provided (a missing name).

import os

error = OSError

_FD_SETSIZE = __pyc_c_call__(int, "_CG_os_const", int, 22)


def _fd(item):
    if isinstance(item, int):
        fd = item
    else:
        fd = item.fileno()
    if fd < 0:
        raise ValueError("file descriptor cannot be a negative integer (" + str(fd) + ")")
    if fd >= _FD_SETSIZE:
        raise ValueError("filedescriptor out of range in select()")
    return fd


def select(rlist, wlist, xlist, timeout=None):
    t = -1.0
    if timeout is not None:
        t = float(timeout)
        if t < 0.0:
            raise ValueError("timeout must be non-negative")
    n = len(rlist) + len(wlist) + len(xlist)
    ps = __pyc_c_call__(bytes, "_CG_poll_new", int, n)
    i = 0
    for item in rlist:
        __pyc_c_call__(int, "_CG_poll_set", bytes, ps, int, i, int, _fd(item), int, 1)
        i += 1
    for item in wlist:
        __pyc_c_call__(int, "_CG_poll_set", bytes, ps, int, i, int, _fd(item), int, 2)
        i += 1
    for item in xlist:
        __pyc_c_call__(int, "_CG_poll_set", bytes, ps, int, i, int, _fd(item), int, 4)
        i += 1
    if __pyc_c_call__(int, "_CG_poll_wait", bytes, ps, float, t) < 0:
        os._fail_fd(__pyc_c_call__(int, "_CG_os_err"))
    res_r = []
    res_w = []
    res_x = []
    i = 0
    for item in rlist:
        if __pyc_c_call__(int, "_CG_poll_ready", bytes, ps, int, i):
            res_r.append(item)
        i += 1
    for item in wlist:
        if __pyc_c_call__(int, "_CG_poll_ready", bytes, ps, int, i):
            res_w.append(item)
        i += 1
    for item in xlist:
        if __pyc_c_call__(int, "_CG_poll_ready", bytes, ps, int, i):
            res_x.append(item)
        i += 1
    return (res_r, res_w, res_x)
