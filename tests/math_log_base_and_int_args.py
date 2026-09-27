# math.log's optional base, and math functions given int arguments
# (the LLVM backend passed the int's bits as a double: sqrt(4) = 2.2e-162).
from math import log
import math
print(log(8.0), log(8, 2), math.log(100, 10), log(0.25, 2))
print(math.sqrt(4), math.exp(0), math.pow(2, 10))
