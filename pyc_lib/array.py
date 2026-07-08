class array:
    def __init__(self, typecode, initializer=None):
        self.typecode = typecode
        self.data = list(initializer) if initializer else []
        
    def append(self, x):
        self.data.append(x)
        
    def extend(self, iterable):
        for x in iterable:
            self.data.append(x)
            
    def __len__(self):
        return len(self.data)
        
    def __getitem__(self, i):
        return self.data[i]
        
    def __setitem__(self, i, v):
        self.data[i] = v
        
    def __iter__(self):
        return iter(self.data)
