# pyc shim for the standard `os` module, POSIX only (issues/202).
#
# A port of what CPython's os and posix modules do for the calls below: the
# same results, and the same OSError subclass and message on failure
# (`[Errno 2] No such file or directory: 'x'`). `os.path` is a real
# submodule, `os/path.py`, CPython's posixpath.
#
# A path argument is a str or an object with __fspath__ (pathlib's paths).
# bytes paths are not supported: bytes has no __fspath__, so one is a
# compile error. The OSError raised carries its message only: `e.errno`,
# `e.strerror` and `e.filename` are not there, which is also a compile
# error (an unresolved member), not a wrong answer.
#
# This module used to be a stub of its own (issues/041): `os.path` was an
# object, `join` took exactly two arguments, a failed mkdir, chdir, remove
# or rename returned an int instead of raising, `stat` returned a plain
# tuple, and `environ` was a dict holding one invented entry.

from . import path

name = 'posix'
sep = '/'
altsep = None
extsep = '.'
pathsep = ':'
linesep = '\n'
curdir = '.'
pardir = '..'
devnull = '/dev/null'
defpath = '/bin:/usr/bin'

F_OK = 0
X_OK = 1
W_OK = 2
R_OK = 4

error = OSError

# open() flags: their values are the platform's, read from C
# (_CG_os_const's table, pyc_runtime.c).
O_RDONLY = __pyc_c_call__(int, "_CG_os_const", int, 0)
O_WRONLY = __pyc_c_call__(int, "_CG_os_const", int, 1)
O_RDWR = __pyc_c_call__(int, "_CG_os_const", int, 2)
O_APPEND = __pyc_c_call__(int, "_CG_os_const", int, 3)
O_CREAT = __pyc_c_call__(int, "_CG_os_const", int, 4)
O_EXCL = __pyc_c_call__(int, "_CG_os_const", int, 5)
O_TRUNC = __pyc_c_call__(int, "_CG_os_const", int, 6)
O_NONBLOCK = __pyc_c_call__(int, "_CG_os_const", int, 7)
O_NOCTTY = __pyc_c_call__(int, "_CG_os_const", int, 8)
O_CLOEXEC = __pyc_c_call__(int, "_CG_os_const", int, 9)
O_SYNC = __pyc_c_call__(int, "_CG_os_const", int, 10)
O_DSYNC = __pyc_c_call__(int, "_CG_os_const", int, 11)
O_DIRECTORY = __pyc_c_call__(int, "_CG_os_const", int, 12)
O_NOFOLLOW = __pyc_c_call__(int, "_CG_os_const", int, 13)
O_ACCMODE = __pyc_c_call__(int, "_CG_os_const", int, 14)
O_NDELAY = __pyc_c_call__(int, "_CG_os_const", int, 15)
_EAGAIN = __pyc_c_call__(int, "_CG_os_const", int, 16)
_EWOULDBLOCK = __pyc_c_call__(int, "_CG_os_const", int, 17)
_EINPROGRESS = __pyc_c_call__(int, "_CG_os_const", int, 18)
_EALREADY = __pyc_c_call__(int, "_CG_os_const", int, 19)


def fspath(p):
    return p.__pyc_fspath__()


def strerror(code):
    return __pyc_c_call__(str, "_CG_strerror", int, code)


# The OSError subclass CPython picks for an errno, with its message. The
# errno values tested as literals are POSIX's and the same on Linux and
# macOS; EAGAIN and its kin differ (11 on Linux, 35 on macOS), so those come
# from C. Any other is a plain OSError, as CPython's is for an errno it has
# no subclass for.
def _raise(e, msg):
    if e == _EAGAIN or e == _EWOULDBLOCK or e == _EINPROGRESS or e == _EALREADY:
        raise BlockingIOError(msg)
    if e == 2:
        raise FileNotFoundError(msg)
    if e == 17:
        raise FileExistsError(msg)
    if e == 1 or e == 13:
        raise PermissionError(msg)
    if e == 20:
        raise NotADirectoryError(msg)
    if e == 21:
        raise IsADirectoryError(msg)
    if e == 4:
        raise InterruptedError(msg)
    if e == 3:
        raise ProcessLookupError(msg)
    if e == 10:
        raise ChildProcessError(msg)
    if e == 32:
        raise BrokenPipeError(msg)
    raise OSError(msg)


def _fail(e, filename):
    _raise(e, "[Errno " + str(e) + "] " + strerror(e) + ": " + repr(filename))


def _fail_fd(e):
    _raise(e, "[Errno " + str(e) + "] " + strerror(e))


def _fail2(e, a, b):
    _raise(e, "[Errno " + str(e) + "] " + strerror(e) + ": " + repr(a) + " -> " + repr(b))


def getcwd():
    return __pyc_c_call__(str, "_CG_getcwd")


def chdir(p):
    p = fspath(p)
    e = __pyc_c_call__(int, "_CG_os_chdir", str, p)
    if e != 0:
        _fail(e, p)


def listdir(p="."):
    p = fspath(p)
    result = []
    h = __pyc_c_call__(int, "_CG_opendir", str, p)
    if h == 0:
        _fail(__pyc_c_call__(int, "_CG_errno"), p)
    while True:
        n = __pyc_c_call__(str, "_CG_readdir_name", int, h)
        if n == "":
            break
        if n != "." and n != "..":
            result.append(n)
    __pyc_c_call__(int, "_CG_closedir", int, h)
    return result


def mkdir(p, mode=0o777):
    p = fspath(p)
    e = __pyc_c_call__(int, "_CG_os_mkdir", str, p, int, mode)
    if e != 0:
        _fail(e, p)


def makedirs(name, mode=0o777, exist_ok=False):
    name = fspath(name)
    head, tail = path.split(name)
    if not tail:
        head, tail = path.split(head)
    if head and tail and not path.exists(head):
        try:
            makedirs(head, exist_ok=exist_ok)
        except FileExistsError:
            # Defeats race condition when another thread created the path
            pass
        if tail == curdir:
            return
    try:
        mkdir(name, mode)
    except OSError:
        if not exist_ok or not path.isdir(name):
            raise


def rmdir(p):
    p = fspath(p)
    e = __pyc_c_call__(int, "_CG_os_rmdir", str, p)
    if e != 0:
        _fail(e, p)


def removedirs(name):
    name = fspath(name)
    rmdir(name)
    head, tail = path.split(name)
    if not tail:
        head, tail = path.split(head)
    while head and tail:
        try:
            rmdir(head)
        except OSError:
            break
        head, tail = path.split(head)


def remove(p):
    p = fspath(p)
    e = __pyc_c_call__(int, "_CG_os_unlink", str, p)
    if e != 0:
        _fail(e, p)


def unlink(p):
    remove(p)


def rename(src, dst):
    src = fspath(src)
    dst = fspath(dst)
    e = __pyc_c_call__(int, "_CG_os_rename", str, src, str, dst)
    if e != 0:
        _fail2(e, src, dst)


def replace(src, dst):
    rename(src, dst)


def symlink(src, dst, target_is_directory=False):
    src = fspath(src)
    dst = fspath(dst)
    e = __pyc_c_call__(int, "_CG_os_symlink", str, src, str, dst)
    if e != 0:
        _fail2(e, src, dst)


def readlink(p):
    p = fspath(p)
    r = __pyc_c_call__(str, "_CG_os_readlink", str, p)
    e = __pyc_c_call__(int, "_CG_os_err")
    if e != 0:
        _fail(e, p)
    return r


def access(p, mode):
    return __pyc_c_call__(int, "_CG_os_access", str, fspath(p), int, mode) != 0


def system(cmd):
    return __pyc_c_call__(int, "system", str, cmd)


# File-descriptor I/O. Descriptors pyc creates are non-inheritable, as
# CPython's are (PEP 446), and an interrupted call is retried (PEP 475).
# dir_fd is not provided (a compile error, not a wrong answer).
def open(path, flags, mode=0o777):
    path = fspath(path)
    fd = __pyc_c_call__(int, "_CG_os_open", str, path, int, flags, int, mode)
    if fd < 0:
        _fail(__pyc_c_call__(int, "_CG_os_err"), path)
    return fd


def read(fd, n):
    data = __pyc_c_call__(bytes, "_CG_os_read", int, fd, int, n)
    e = __pyc_c_call__(int, "_CG_os_err")
    if e != 0:
        _fail_fd(e)
    return data


def _bytes_like(data):
    if isinstance(data, str):
        raise TypeError("a bytes-like object is required, not 'str'")
    if isinstance(data, int):
        raise TypeError("a bytes-like object is required, not 'int'")
    return bytes(data)


def write(fd, data):
    n = __pyc_c_call__(int, "_CG_os_write", int, fd, bytes, _bytes_like(data))
    if n < 0:
        _fail_fd(__pyc_c_call__(int, "_CG_os_err"))
    return n


def close(fd):
    if __pyc_c_call__(int, "_CG_os_close", int, fd) != 0:
        _fail_fd(__pyc_c_call__(int, "_CG_os_err"))


def _fd_pair(p):
    if p < 0:
        _fail_fd(__pyc_c_call__(int, "_CG_os_err"))
    return (p & 0xffffffff, p >> 32)


def pipe():
    return _fd_pair(__pyc_c_call__(int, "_CG_os_pipe"))


def openpty():
    return _fd_pair(__pyc_c_call__(int, "_CG_os_openpty"))


def isatty(fd):
    return __pyc_c_call__(int, "_CG_os_isatty", int, fd) != 0


def get_blocking(fd):
    r = __pyc_c_call__(int, "_CG_os_get_blocking", int, fd)
    if r < 0:
        _fail_fd(__pyc_c_call__(int, "_CG_os_err"))
    return r == 1


def set_blocking(fd, blocking):
    if __pyc_c_call__(int, "_CG_os_set_blocking", int, fd, int, 1 if blocking else 0) != 0:
        _fail_fd(__pyc_c_call__(int, "_CG_os_err"))


def getpid():
    return __pyc_c_call__(int, "getpid")


def cpu_count():
    return __pyc_c_call__(int, "_CG_os_cpu_count")


# os.stat_result. Indexing and iteration give CPython's ten-field tuple view
# (mode, ino, dev, nlink, uid, gid, size, atime, mtime, ctime), with the
# times in whole seconds; the st_*time attributes are floats and the
# st_*time_ns attributes ints, as CPython's are.
class stat_result:
    def __init__(self):
        self.st_mode = __pyc_c_call__(int, "_CG_os_stat_int", int, 0)
        self.st_ino = __pyc_c_call__(int, "_CG_os_stat_int", int, 1)
        self.st_dev = __pyc_c_call__(int, "_CG_os_stat_int", int, 2)
        self.st_nlink = __pyc_c_call__(int, "_CG_os_stat_int", int, 3)
        self.st_uid = __pyc_c_call__(int, "_CG_os_stat_int", int, 4)
        self.st_gid = __pyc_c_call__(int, "_CG_os_stat_int", int, 5)
        self.st_size = __pyc_c_call__(int, "_CG_os_stat_int", int, 6)
        self.st_atime = __pyc_c_call__(float, "_CG_os_stat_time", int, 0)
        self.st_mtime = __pyc_c_call__(float, "_CG_os_stat_time", int, 1)
        self.st_ctime = __pyc_c_call__(float, "_CG_os_stat_time", int, 2)
        self.st_atime_ns = __pyc_c_call__(int, "_CG_os_stat_int", int, 10)
        self.st_mtime_ns = __pyc_c_call__(int, "_CG_os_stat_int", int, 11)
        self.st_ctime_ns = __pyc_c_call__(int, "_CG_os_stat_int", int, 12)
        self.st_blocks = __pyc_c_call__(int, "_CG_os_stat_int", int, 13)
        self.st_blksize = __pyc_c_call__(int, "_CG_os_stat_int", int, 14)
        self.st_rdev = __pyc_c_call__(int, "_CG_os_stat_int", int, 15)
        self._t = (self.st_mode, self.st_ino, self.st_dev, self.st_nlink,
                   self.st_uid, self.st_gid, self.st_size,
                   __pyc_c_call__(int, "_CG_os_stat_int", int, 7),
                   __pyc_c_call__(int, "_CG_os_stat_int", int, 8),
                   __pyc_c_call__(int, "_CG_os_stat_int", int, 9))

    def __getitem__(self, i):
        return self._t[i]

    def __len__(self):
        return 10

    def __iter__(self):
        return iter(self._t)

    def __eq__(self, other):
        return self._t == other._t

    def __repr__(self):
        t = self._t
        return ("os.stat_result(st_mode=" + str(t[0]) + ", st_ino=" + str(t[1]) +
                ", st_dev=" + str(t[2]) + ", st_nlink=" + str(t[3]) +
                ", st_uid=" + str(t[4]) + ", st_gid=" + str(t[5]) +
                ", st_size=" + str(t[6]) + ", st_atime=" + str(t[7]) +
                ", st_mtime=" + str(t[8]) + ", st_ctime=" + str(t[9]) + ")")


def stat(p, follow_symlinks=True):
    p = fspath(p)
    e = __pyc_c_call__(int, "_CG_os_stat", str, p, int, 1 if follow_symlinks else 0)
    if e != 0:
        _fail(e, p)
    return stat_result()


def lstat(p):
    return stat(p, follow_symlinks=False)


# os.walk, CPython's algorithm: list the directory, split its entries into
# directories (following symlinks, as is_dir() does) and the rest, and
# descend into each directory that is not a symlink unless followlinks.
# Top-down, the caller may prune `dirnames` in place before the descent.
def walk(top, topdown=True, onerror=None, followlinks=False):
    top = fspath(top)
    try:
        names = listdir(top)
    except OSError as err:
        if onerror is not None:
            onerror(err)
        return
    dirs = []
    nondirs = []
    walk_into = []
    for n in names:
        full = path.join(top, n)
        if path.isdir(full):
            dirs.append(n)
            if not topdown and (followlinks or not path.islink(full)):
                walk_into.append(full)
        else:
            nondirs.append(n)
    if topdown:
        yield (top, dirs, nondirs)
        for d in dirs:
            new_path = path.join(top, d)
            if followlinks or not path.islink(new_path):
                yield from walk(new_path, topdown, onerror, followlinks)
    else:
        for new_path in walk_into:
            yield from walk(new_path, topdown, onerror, followlinks)
        yield (top, dirs, nondirs)


# os.environ: the process environment when the program started, and a
# write or delete passes through to setenv/unsetenv as CPython's does, so a
# child process (os.system) sees it.
class _Environ:
    def __init__(self):
        self._data = {}
        i = 0
        while True:
            kv = __pyc_c_call__(str, "_CG_os_environ_at", int, i)
            if kv == "":
                break
            j = kv.find("=")
            if j >= 0:
                k = kv[:j]
                if k not in self._data:
                    self._data[k] = kv[j + 1:]
            i += 1

    def __getitem__(self, key):
        if key not in self._data:
            raise KeyError(repr(key))
        return self._data[key]

    def __setitem__(self, key, value):
        e = __pyc_c_call__(int, "_CG_os_setenv", str, key, str, value)
        if e != 0:
            _raise(e, "[Errno " + str(e) + "] " + strerror(e))
        self._data[key] = value

    def __delitem__(self, key):
        if key not in self._data:
            raise KeyError(repr(key))
        __pyc_c_call__(int, "_CG_os_unsetenv", str, key)
        del self._data[key]

    def __contains__(self, key):
        return key in self._data

    def __iter__(self):
        return iter(list(self._data.keys()))

    def __len__(self):
        return len(self._data)

    def get(self, key, default=None):
        if key in self._data:
            return self._data[key]
        return default

    def keys(self):
        return self._data.keys()

    def values(self):
        return self._data.values()

    def items(self):
        return self._data.items()

    def copy(self):
        return dict(self._data)

    def setdefault(self, key, value):
        if key not in self._data:
            self[key] = value
        return self._data[key]

    # Not through self[key]: a raise inside an item subscript does not
    # reach the caller's handler yet (issues/175), and a call's does.
    def pop(self, key):
        if key not in self._data:
            raise KeyError(repr(key))
        v = self._data[key]
        __pyc_c_call__(int, "_CG_os_unsetenv", str, key)
        del self._data[key]
        return v

    def __repr__(self):
        parts = []
        for k in self._data:
            parts.append(repr(k) + ": " + repr(self._data[k]))
        return "environ({" + ", ".join(parts) + "})"


environ = _Environ()


def getenv(key, default=None):
    return environ.get(key, default)


def putenv(key, value):
    e = __pyc_c_call__(int, "_CG_os_setenv", str, key, str, value)
    if e != 0:
        _raise(e, "[Errno " + str(e) + "] " + strerror(e))


def unsetenv(key):
    __pyc_c_call__(int, "_CG_os_unsetenv", str, key)
