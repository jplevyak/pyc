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

# A no-op: sleeping does not affect program results, only wall time,
# and a real sleep would make tests slow/nondeterministic.
def sleep(seconds):
    return None
