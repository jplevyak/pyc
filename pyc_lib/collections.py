class deque:
    def __init__(self, iterable=None):
        if iterable:
            self.d = [x for x in iterable]
        else:
            self.d = []
            
    def append(self, item):
        self.d.append(item)
        
    def popleft(self):
        return self.d.pop(0)
        
    def pop(self):
        return self.d.pop()
        
    def __len__(self):
        return len(self.d)
        
    def __iter__(self):
        return iter(self.d)

class defaultdict:
    def __init__(self, factory=None, initial=None):
        self.factory = factory
        self.d = {}
        if initial:
            for k in initial:
                self.d[k] = initial[k]

    def __getitem__(self, key):
        if key not in self.d:
            if self.factory:
                self.d[key] = self.factory()
            else:
                self.d[key] = None
        return self.d[key]
        
    def __setitem__(self, key, value):
        self.d[key] = value

    def __contains__(self, key):
        return key in self.d
