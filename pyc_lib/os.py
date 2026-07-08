# pyc shim for the standard `os` module

class _os_path:
    def isdir(self, p): return True
    def exists(self, p): return True
    def dirname(self, p): return ""
    def splitext(self, p): return (p, "")
    def join(self, a, b): return a + "/" + b

path = _os_path()
environ = {'SDL_VIDEO_CENTERED': '1'}

def listdir(p): return []
def system(cmd): return 0
def walk(top): return []
