# ifa/211: under -j (the JIT) any generator aborted in LLVM ("Do not know how
# to promote this operator's operand!"): the module reached LLJIT without the
# optimization pipeline that lowers its coroutines. And the JIT called main
# with no argv, so len(sys.argv) was 0. Run with -j (.flags); the .check is
# CPython's output.
import sys


def count(n):
    for i in range(n):
        yield i


def chain(a, b):
    yield from a
    yield from b


print(sum(count(4)))
print(list(chain(count(2), count(3))))
print(sum(i * i for i in range(10)))
print(len(sys.argv), sys.argv[0].endswith("jit_generator.py"))
