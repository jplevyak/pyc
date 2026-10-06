# A __pyc_c_call__ argument whose type contradicts the declared one is a
# compile error, found by FA. It used to be caught only in codegen, which
# under the default permissive mode emitted a runtime assert and no
# diagnostic: pygasus's ord(bytes) compiled silently and aborted at startup.
def myord(x):
    return __pyc_c_call__(int, "_CG_ord", str, x)

print(myord("A"))
print(myord(b"A"))
