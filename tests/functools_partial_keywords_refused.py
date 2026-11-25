# A partial with keyword arguments needs '**' into named parameters
# (issues/189 step 2), so it is refused where it is built.
from functools import partial


def power(base, exp):
    return base ** exp


square = partial(power, exp=2)
print(square(3))
