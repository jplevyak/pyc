# Memory Buffer implementation built on Micro-Core FFI

class MemoryBuffer:
    def __init__(self, size):
        self.size = size
        self.ptr = __pyc_c_call__(int, "_CG_FFI_Alloc", int, size)
        if not self.ptr:
            # Memory allocation failed. Micro-Core Exit.
            __pyc_c_call__(int, "_CG_Syscall_Exit", int, 1)

    def free(self):
        if self.ptr:
            __pyc_c_call__(int, "_CG_FFI_Free", int, self.ptr)
            self.ptr = 0
            self.size = 0

    def get_int64(self, offset):
        if offset < 0 or offset + 8 > self.size:
            # Out of bounds
            __pyc_c_call__(int, "_CG_Syscall_Exit", int, 2)
        return __pyc_c_call__(int, "_CG_FFI_Get_Int64", int, self.ptr, int, offset)

    def set_int64(self, offset, val):
        if offset < 0 or offset + 8 > self.size:
            # Out of bounds
            __pyc_c_call__(int, "_CG_Syscall_Exit", int, 2)
        __pyc_c_call__(int, "_CG_FFI_Set_Int64", int, self.ptr, int, offset, int, val)

    def get_int8(self, offset):
        if offset < 0 or offset + 1 > self.size:
            # Out of bounds
            __pyc_c_call__(int, "_CG_Syscall_Exit", int, 2)
        return __pyc_c_call__(int, "_CG_FFI_Get_Int8", int, self.ptr, int, offset)

    def set_int8(self, offset, val):
        if offset < 0 or offset + 1 > self.size:
            # Out of bounds
            __pyc_c_call__(int, "_CG_Syscall_Exit", int, 2)
        __pyc_c_call__(int, "_CG_FFI_Set_Int8", int, self.ptr, int, offset, int, val)
