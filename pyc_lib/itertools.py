class count:
    def __init__(self, start=0, step=1):
        self.n = start
        self.step = step
        
    def __iter__(self):
        return self
        
    def next(self):
        v = self.n
        self.n += self.step
        return v

def product(A, B=None, C=None, D=None):
    result = []
    if B is None:
        for a in A:
            result.append((a,))
    elif C is None:
        for a in A:
            for b in B:
                result.append((a, b))
    elif D is None:
        for a in A:
            for b in B:
                for c in C:
                    result.append((a, b, c))
    else:
        for a in A:
            for b in B:
                for c in C:
                    for d in D:
                        result.append((a, b, c, d))
    return result
