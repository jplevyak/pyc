import lib.memory

class Dict:
    def __init__(self):
        self.capacity = 8
        self.length = 0
        self.keys = lib.memory.MemoryBuffer(self.capacity * 8)
        self.values = lib.memory.MemoryBuffer(self.capacity * 8)
        # Using 0 as empty key (assuming integer keys for now).
        # A full Pyc implementation will use object pointers and handle __hash__
        self._clear_keys()

    def _clear_keys(self):
        i = 0
        while i < self.capacity:
            self.keys.set_int64(i * 8, 0)
            i += 1

    def _hash(self, key):
        # Extremely basic hash for integers
        return (key * 2654435761) % self.capacity

    def put(self, key, value):
        if key == 0:
            # 0 is reserved for empty slots in this simple int dict prototype
            __pyc_c_call__(int, "_CG_Syscall_Exit", int, 4)

        if self.length >= self.capacity // 2:
            self._grow()
            
        h = self._hash(key)
        while True:
            k = self.keys.get_int64(h * 8)
            if k == 0 or k == key:
                if k == 0:
                    self.length += 1
                self.keys.set_int64(h * 8, key)
                self.values.set_int64(h * 8, value)
                return
            h = (h + 1) % self.capacity

    def get(self, key):
        if key == 0:
            return 0
        h = self._hash(key)
        while True:
            k = self.keys.get_int64(h * 8)
            if k == 0:
                return 0 # not found
            if k == key:
                return self.values.get_int64(h * 8)
            h = (h + 1) % self.capacity

    def _grow(self):
        old_cap = self.capacity
        old_keys = self.keys
        old_values = self.values
        
        self.capacity = old_cap * 2
        self.keys = lib.memory.MemoryBuffer(self.capacity * 8)
        self.values = lib.memory.MemoryBuffer(self.capacity * 8)
        self._clear_keys()
        self.length = 0
        
        i = 0
        while i < old_cap:
            k = old_keys.get_int64(i * 8)
            if k != 0:
                self.put(k, old_values.get_int64(i * 8))
            i += 1
            
        old_keys.free()
        old_values.free()

    def free(self):
        self.keys.free()
        self.values.free()
