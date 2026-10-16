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
