import lib.memory

class List:
    def __init__(self):
        self.capacity = 4
        self.length = 0
        # Allocate capacity for 8-byte elements (assuming int64/pointers)
        self.buffer = lib.memory.MemoryBuffer(self.capacity * 8)

    def append(self, item):
        if self.length == self.capacity:
            self._grow()
        self.buffer.set_int64(self.length * 8, item)
        self.length += 1

    def __getitem__(self, index):
        if index < 0 or index >= self.length:
            # Bounds check failed
            __pyc_c_call__(int, "_CG_Syscall_Exit", int, 3)
            return 0
        return self.buffer.get_int64(index * 8)

    def __setitem__(self, index, item):
        if index < 0 or index >= self.length:
            __pyc_c_call__(int, "_CG_Syscall_Exit", int, 3)
            return
        self.buffer.set_int64(index * 8, item)
        
    def __len__(self):
        return self.length

    def _grow(self):
        new_cap = self.capacity * 2
        new_buf = lib.memory.MemoryBuffer(new_cap * 8)
        
        # Copy elements manually (in the future we can bind a _CG_Memory_Copy primitive)
        i = 0
        while i < self.length:
            val = self.buffer.get_int64(i * 8)
            new_buf.set_int64(i * 8, val)
            i += 1
            
        self.buffer.free()
        self.buffer = new_buf
        self.capacity = new_cap

    def free(self):
        self.buffer.free()
