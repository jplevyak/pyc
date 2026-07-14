# pyc shim for the standard `os` module

def _str_sub(s, i, j):
    # s[i:j] built via single-char indexing + concat: str has no
    # working slice path yet (see __pyc__/01_str.py __contains__).
    r = ""
    k = i
    while k < j:
        r = r + s[k]
        k += 1
    return r

class _os_path:
    def isdir(self, p): return True
    def islink(self, p): return False
    def exists(self, p): return True
    def dirname(self, p):
        head, tail = self.split(p)
        return head
    def basename(self, p):
        head, tail = self.split(p)
        return tail
    def split(self, p):
        n = len(p)
        i = n - 1
        while i >= 0 and p[i] != '/':
            i -= 1
        if i < 0:
            return ("", p)
        return (_str_sub(p, 0, i), _str_sub(p, i + 1, n))
    def splitext(self, p):
        n = len(p)
        slash = -1
        i = n - 1
        while i >= 0:
            if p[i] == '/':
                slash = i
                break
            i -= 1
        dot = -1
        i = n - 1
        while i > slash:
            if p[i] == '.':
                dot = i
                break
            i -= 1
        if dot < 0 or dot <= slash + 1:
            return (p, "")
        return (_str_sub(p, 0, dot), _str_sub(p, dot, n))
    def join(self, a, b): return a + "/" + b

path = _os_path()
environ = {'SDL_VIDEO_CENTERED': '1'}

def listdir(p): return []
def system(cmd): return 0
def walk(top): return []
def chdir(p): return 0
def rename(a, b): return 0
def remove(p): return 0
def mkdir(p): return 0
def getcwd(): return ""
def stat(p): return (0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
