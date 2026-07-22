# issue 025 / path_tracing: float("...") and int("...") of a string.
# The synthesized __coerce__ method lowered a string argument to a raw
# (double)(char*) cast (a C compile error; an LLVM verifier failure).
# Codegen now routes a string source through the runtime parser
# (_CG_str_to_float64 / _CG_str_to_int64). Values chosen to be exactly
# representable so str(float) matches CPython byte-for-byte.
print(float("inf"))
print(float("-inf"))
print(float("2.5"))
print(float("2.5") + 1.0)
print(float("inf") > 1e308)
print(int("42"))
print(int("-7"))
print(int("100") + 1)
print(int(float("3.0")))

def parse(s):
    return float(s) * 2.0

print(parse("1.5"))
print(parse("0.25"))
