# pyc shim for the standard `time` module. Enough for the common
# benchmarking use (time.time() deltas). Wall-clock resolution is
# whole seconds (libc time()); sub-second timing is not modelled.
def time():
    return float(__pyc_c_call__(int, "time", int, 0))

# A no-op: sleeping does not affect program results, only wall time,
# and a real sleep would make tests slow/nondeterministic.
def sleep(seconds):
    return None
