def test_alloc():
    ptr = __pyc_c_call__(int, "_CG_FFI_Alloc", int, 16)
    return ptr

print("Allocated at:", test_alloc())
