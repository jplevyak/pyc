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
    def isdir(self, p): return __pyc_c_call__(bool, "_CG_is_dir", str, p)
    def islink(self, p): return __pyc_c_call__(bool, "_CG_is_symlink", str, p)
    def exists(self, p): return __pyc_c_call__(int, "access", str, p, int, 0) == 0
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

# issues/041: listdir/system/walk/chdir/rename/remove/mkdir/getcwd/stat
# were no-op stubs (listdir/walk always [], stat always all-zero,
# system/chdir/rename/remove/mkdir always "succeeded" without doing
# anything, getcwd always ""). A pyc `_CG_string` is already a valid
# NUL-terminated `const char*`, so most of these call the real libc
# function directly by name via __pyc_c_call__ -- no C-side wrapper
# needed. See pyc_c_runtime.h for the handful that do need one
# (getcwd's fixed buffer, opendir/readdir's pointer-through-int64
# handle, stat's struct fields).

def listdir(p):
    result = []
    h = __pyc_c_call__(int, "_CG_opendir", str, p)
    if h == 0:
        return result
    while True:
        name = __pyc_c_call__(str, "_CG_readdir_name", int, h)
        if name == "":
            break
        if name != "." and name != "..":
            result.append(name)
    __pyc_c_call__(int, "_CG_closedir", int, h)
    return result

def system(cmd):
    return __pyc_c_call__(int, "system", str, cmd)

def walk(top):
    # Non-recursive (explicit stack) so this stays an ordinary
    # function rather than depending on recursive-generator support;
    # top-down, matching CPython's default os.walk(top) order.
    result = []
    stack = [top]
    while stack:
        d = stack.pop()
        dirnames = []
        filenames = []
        for name in listdir(d):
            full = path.join(d, name)
            if path.isdir(full):
                dirnames.append(name)
            else:
                filenames.append(name)
        result.append((d, dirnames, filenames))
        for name in dirnames:
            stack.append(path.join(d, name))
    return result

def chdir(p):
    return __pyc_c_call__(int, "chdir", str, p)

def rename(a, b):
    return __pyc_c_call__(int, "rename", str, a, str, b)

def remove(p):
    return __pyc_c_call__(int, "unlink", str, p)

def mkdir(p):
    return __pyc_c_call__(int, "mkdir", str, p, int, 0o777)

def getcwd():
    return __pyc_c_call__(str, "_CG_getcwd")

def stat(p):
    return (
        __pyc_c_call__(int, "_CG_stat_int_field", str, p, int, 0),
        __pyc_c_call__(int, "_CG_stat_int_field", str, p, int, 1),
        __pyc_c_call__(int, "_CG_stat_int_field", str, p, int, 2),
        __pyc_c_call__(int, "_CG_stat_int_field", str, p, int, 3),
        __pyc_c_call__(int, "_CG_stat_int_field", str, p, int, 4),
        __pyc_c_call__(int, "_CG_stat_int_field", str, p, int, 5),
        __pyc_c_call__(int, "_CG_stat_int_field", str, p, int, 6),
        __pyc_c_call__(int, "_CG_stat_int_field", str, p, int, 7),
        __pyc_c_call__(int, "_CG_stat_int_field", str, p, int, 8),
        __pyc_c_call__(int, "_CG_stat_int_field", str, p, int, 9),
    )
