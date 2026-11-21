# pyc shim for the standard `time` module. Enough for the common
# benchmarking use (time.time() deltas).
#
# issues/164: this used libc `time(0)`, i.e. WHOLE SECONDS, so every
# self-timing corpus program printed a quantised or zero elapsed time --
# `sieve` reported "time: 0.00" against CPython's 0.85, `tictactoe`
# "TIME 3.00" against 0.96, and `pystone` divided its loop count by a
# zero-or-one-second delta and claimed a flat "1000000.000000
# pystones/second". The runtime has always exported a microsecond clock
# (`_CG_get_time`, gettimeofday, pyc_runtime.c) for the async timer
# queue; time.time() simply never used it.
def time():
    return __pyc_c_call__(float, "_CG_get_time")

# issues/041: this was a no-op, which is observable (a `time.time()` delta
# around it, or any program pacing itself against another process). CPython
# raises ValueError for a negative duration.
def sleep(seconds):
    if seconds < 0:
        raise ValueError("sleep length must be non-negative")
    __pyc_c_call__(int, "_CG_sleep", float, seconds)
    return None

# The clocks, as CPython's (issues/207): time_ns is CLOCK_REALTIME,
# monotonic and perf_counter CLOCK_MONOTONIC, process_time the process's
# CPU clock, each in seconds as a float or in integer nanoseconds.
def time_ns():
    return __pyc_c_call__(int, "_CG_clock_ns", int, 0)


def monotonic_ns():
    return __pyc_c_call__(int, "_CG_clock_ns", int, 1)


def perf_counter_ns():
    return __pyc_c_call__(int, "_CG_clock_ns", int, 1)


def process_time_ns():
    return __pyc_c_call__(int, "_CG_clock_ns", int, 2)


def monotonic():
    return monotonic_ns() / 1e9


def perf_counter():
    return perf_counter_ns() / 1e9


def process_time():
    return process_time_ns() / 1e9


# time.struct_time: the nine fields by name and as a tuple, plus tm_zone and
# tm_gmtoff, with CPython's repr.
class struct_time:
    def __init__(self, year, mon, mday, hour, minute, sec, wday, yday, isdst, zone="", gmtoff=0):
        self.tm_year = year
        self.tm_mon = mon
        self.tm_mday = mday
        self.tm_hour = hour
        self.tm_min = minute
        self.tm_sec = sec
        self.tm_wday = wday
        self.tm_yday = yday
        self.tm_isdst = isdst
        self.tm_zone = zone
        self.tm_gmtoff = gmtoff
        self._t = (year, mon, mday, hour, minute, sec, wday, yday, isdst)

    def __getitem__(self, i):
        return self._t[i]

    def __len__(self):
        return 9

    def __iter__(self):
        return iter(self._t)

    def __eq__(self, other):
        return self._t == other._t

    def __ne__(self, other):
        return not self.__eq__(other)

    def __repr__(self):
        t = self._t
        return ("time.struct_time(tm_year=" + str(t[0]) + ", tm_mon=" + str(t[1]) +
                ", tm_mday=" + str(t[2]) + ", tm_hour=" + str(t[3]) + ", tm_min=" + str(t[4]) +
                ", tm_sec=" + str(t[5]) + ", tm_wday=" + str(t[6]) + ", tm_yday=" + str(t[7]) +
                ", tm_isdst=" + str(t[8]) + ")")


def _floor_secs(secs):
    f = float(secs)
    if f != f:
        raise ValueError("Invalid value NaN (not a number)")
    i = int(f)
    if float(i) > f:
        i -= 1
    return i


def _struct(secs, local):
    if secs is None:
        s = _floor_secs(time())
    else:
        s = _floor_secs(secs)
    e = __pyc_c_call__(int, "_CG_time_tm", int, s, int, 1 if local else 0)
    if e != 0:
        raise OverflowError("timestamp out of range for platform time_t")
    f = []
    for i in range(10):
        f.append(__pyc_c_call__(int, "_CG_time_tm_field", int, i))
    return struct_time(f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8],
                       __pyc_c_call__(str, "_CG_time_tm_zone"), f[9])


def gmtime(secs=None):
    return _struct(secs, False)


def localtime(secs=None):
    return _struct(secs, True)


# CPython's checktm: what strftime accepts, with 0 for a month or day of
# month read as 1 (gettmarg and checktm).
def _checked(t):
    mon = t[1]
    if mon == 0:
        mon = 1
    elif mon < 1 or mon > 12:
        raise ValueError("month out of range")
    mday = t[2]
    if mday == 0:
        mday = 1
    elif mday < 0 or mday > 31:
        raise ValueError("day of month out of range")
    if t[3] < 0 or t[3] > 23:
        raise ValueError("hour out of range")
    if t[4] < 0 or t[4] > 59:
        raise ValueError("minute out of range")
    if t[5] < 0 or t[5] > 61:
        raise ValueError("seconds out of range")
    # gettmarg takes (wday + 1) % 7 with C's remainder, which keeps the
    # dividend's sign; checktm refuses only a negative result.
    cw = t[6] + 1
    r = cw - 7 * int(cw / 7)
    if r < 0:
        raise ValueError("day of week out of range")
    wday = (r + 6) % 7
    yday = t[7]
    if yday == 0:
        yday = 1
    elif yday < 0 or yday > 366:
        raise ValueError("day of year out of range")
    return (t[0], mon, mday, t[3], t[4], t[5], wday, yday, t[8])


def _zone_of(t):
    if isinstance(t, struct_time):
        return t.tm_zone
    return ""


def _gmtoff_of(t):
    if isinstance(t, struct_time):
        return t.tm_gmtoff
    return 0


# strftime(format) of the current local time, or of a struct_time or
# 9-tuple (issues/197, issues/207).
def strftime(format, t=None):
    if t is None:
        return __pyc_c_call__(str, "_CG_strftime_local", str, format, float, time())
    c = _checked(t)
    return __pyc_c_call__(str, "_CG_strftime_fields", str, format, int, c[0], int, c[1], int, c[2],
                          int, c[3], int, c[4], int, c[5], int, c[6], int, c[7], int, c[8],
                          int, _gmtoff_of(t), str, _zone_of(t))


def mktime(t):
    r = __pyc_c_call__(float, "_CG_mktime_fields", int, t[0], int, t[1], int, t[2], int, t[3],
                       int, t[4], int, t[5], int, t[6], int, t[7], int, t[8])
    if r == -1.0e300:
        raise OverflowError("mktime argument out of range")
    return r


_days = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
_months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]


def _two(n):
    return ("0" + str(n)) if n < 10 else str(n)


# CPython's _asctime: "Sun Sep  9 01:46:40 2001", the day of month in three
# columns.
def asctime(t=None):
    if t is None:
        t = localtime()
    c = _checked(t)
    d = str(c[2])
    if len(d) < 3:
        d = " " * (3 - len(d)) + d
    return (_days[c[6]] + " " + _months[c[1] - 1] + d + " " + _two(c[3]) + ":" + _two(c[4]) +
            ":" + _two(c[5]) + " " + str(c[0]))


def ctime(secs=None):
    return asctime(localtime(secs))
