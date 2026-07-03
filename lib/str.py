import lib.memory

class String:
    def __init__(self, size=0):
        self.length = size
        # Allocate bytes + 1 for null terminator
        self.buffer = lib.memory.MemoryBuffer(size + 1)
        if size == 0:
            self.buffer.set_int8(0, 0)

    def set_char(self, index, char_code):
        if index < 0 or index >= self.length:
            __pyc_c_call__(int, "_CG_Syscall_Exit", int, 3)
        self.buffer.set_int8(index, char_code)

    def get_char(self, index):
        if index < 0 or index >= self.length:
            __pyc_c_call__(int, "_CG_Syscall_Exit", int, 3)
        return self.buffer.get_int8(index)

    def __add__(self, other):
        # A simple concatenation operator using raw memory
        new_len = self.length + other.length
        new_str = String(new_len)
        
        # Copy self
        i = 0
        while i < self.length:
            new_str.set_char(i, self.get_char(i))
            i += 1
            
        # Copy other
        j = 0
        while j < other.length:
            new_str.set_char(i + j, other.get_char(j))
            j += 1
            
        new_str.buffer.set_int8(new_len, 0) # null terminate
        return new_str

    def free(self):
        self.buffer.free()
