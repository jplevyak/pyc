# issue 030: `with` never called `__exit__` when the body raised --
# exceptions unwound straight past every enclosing context manager,
# silently dropping cleanup side effects and ignoring `__exit__`'s
# ability to suppress the exception via a truthy return. Fixed by
# routing the with-item body through a real try_stack frame (mirrors
# PY_try_stmt's own mechanism), so a raise inside now reaches
# `__exit__` with the real exception value and its return value gates
# suppression vs. re-propagation, matching CPython's PEP 343
# desugaring.

class Suppress:
    def __enter__(self):
        print("enter-suppress")
    def __exit__(self, a, b, c):
        print("exit-suppress", b)
        return True

class NoSuppress:
    def __enter__(self):
        print("enter-nosuppress")
    def __exit__(self, a, b, c):
        print("exit-nosuppress", b)
        return False

def raises_suppressed():
    with Suppress():
        print("body-suppressed")
        raise ValueError("boom1")
    print("after-suppressed")

def raises_propagated():
    with NoSuppress():
        print("body-propagated")
        raise ValueError("boom2")
    print("after-propagated")  # unreachable

def raises_nested():
    with NoSuppress():
        with NoSuppress():
            print("body-nested")
            raise ValueError("boom3")
        print("after-inner")  # unreachable
    print("after-outer")  # unreachable

def raises_comma_form_inner_suppresses():
    with NoSuppress(), Suppress():
        print("body-comma")
        raise ValueError("boom4")
    print("after-comma")

print("== suppressed ==")
raises_suppressed()

print("== propagated ==")
try:
    raises_propagated()
except ValueError as e:
    print("caught:", e)

print("== nested ==")
try:
    raises_nested()
except ValueError as e:
    print("caught:", e)

print("== comma form, inner suppresses ==")
raises_comma_form_inner_suppresses()

print("done")
