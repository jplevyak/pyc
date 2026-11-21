# pyc shim for `os.path`: a port of CPython's posixpath (issues/202).
#
# Each function takes a str or an object with __fspath__, as CPython's do.
# bytes paths are not supported (see os/__init__.py).

import os

sep = '/'
altsep = None
extsep = '.'
pathsep = ':'
curdir = '.'
pardir = '..'
defpath = '/bin:/usr/bin'
devnull = '/dev/null'

_S_IFMT = 0o170000
_S_IFDIR = 0o040000
_S_IFREG = 0o100000
_S_IFLNK = 0o120000


def normcase(s):
    return os.fspath(s)


def isabs(s):
    s = os.fspath(s)
    return s.startswith(sep)


def join(a, *p):
    path = os.fspath(a)
    for b in p:
        b = os.fspath(b)
        if b.startswith(sep):
            path = b
        elif not path or path.endswith(sep):
            path += b
        else:
            path += sep + b
    return path


def split(p):
    p = os.fspath(p)
    i = p.rfind(sep) + 1
    head = p[:i]
    tail = p[i:]
    if head and head != sep * len(head):
        head = head.rstrip(sep)
    return (head, tail)


def splitext(p):
    p = os.fspath(p)
    sep_index = p.rfind(sep)
    dot_index = p.rfind(extsep)
    if dot_index > sep_index:
        # skip all leading dots
        filename_index = sep_index + 1
        while filename_index < dot_index:
            if p[filename_index] != extsep:
                return (p[:dot_index], p[dot_index:])
            filename_index += 1
    return (p, "")


def splitdrive(p):
    p = os.fspath(p)
    return ("", p)


def splitroot(p):
    p = os.fspath(p)
    if p[:1] != sep:
        return ("", "", p)
    elif p[1:2] != sep or p[2:3] == sep:
        return ("", sep, p[1:])
    else:
        return ("", p[:2], p[2:])


def basename(p):
    p = os.fspath(p)
    i = p.rfind(sep) + 1
    return p[i:]


def dirname(p):
    p = os.fspath(p)
    i = p.rfind(sep) + 1
    head = p[:i]
    if head and head != sep * len(head):
        head = head.rstrip(sep)
    return head


# The predicates call stat themselves rather than os.stat, so a missing
# path is False without an exception raised and caught.
def _mode(p, follow):
    if __pyc_c_call__(int, "_CG_os_stat", str, os.fspath(p), int, follow) != 0:
        return -1
    return __pyc_c_call__(int, "_CG_os_stat_int", int, 0)


def exists(p):
    return _mode(p, 1) >= 0


def lexists(p):
    return _mode(p, 0) >= 0


def isdir(p):
    m = _mode(p, 1)
    return m >= 0 and (m & _S_IFMT) == _S_IFDIR


def isfile(p):
    m = _mode(p, 1)
    return m >= 0 and (m & _S_IFMT) == _S_IFREG


def islink(p):
    m = _mode(p, 0)
    return m >= 0 and (m & _S_IFMT) == _S_IFLNK


def getsize(p):
    return os.stat(p).st_size


def getmtime(p):
    return os.stat(p).st_mtime


def getatime(p):
    return os.stat(p).st_atime


def getctime(p):
    return os.stat(p).st_ctime


def samefile(f1, f2):
    s1 = os.stat(f1)
    s2 = os.stat(f2)
    return s1.st_ino == s2.st_ino and s1.st_dev == s2.st_dev


def expanduser(path):
    path = os.fspath(path)
    if not path.startswith('~'):
        return path
    i = path.find(sep, 1)
    if i < 0:
        i = len(path)
    if i == 1:
        if 'HOME' not in os.environ:
            userhome = __pyc_c_call__(str, "_CG_os_pw_home", str, "")
            if userhome == "":
                return path
        else:
            userhome = os.environ['HOME']
    else:
        userhome = __pyc_c_call__(str, "_CG_os_pw_home", str, path[1:i])
        if userhome == "":
            return path
    userhome = userhome.rstrip(sep)
    return (userhome + path[i:]) or sep


def _is_word(c):
    return ('a' <= c and c <= 'z') or ('A' <= c and c <= 'Z') or ('0' <= c and c <= '9') or c == '_'


# CPython matches `\$(\w+|\{[^}]*\})` (re.ASCII) and replaces each match
# whose name is in os.environ, leaving the others as they are.
def expandvars(path):
    path = os.fspath(path)
    if '$' not in path:
        return path
    i = 0
    while True:
        start = path.find('$', i)
        if start < 0:
            break
        j = start + 1
        n = len(path)
        if j < n and path[j] == '{':
            close = path.find('}', j + 1)
            if close < 0:
                i = start + 1
                continue
            name = path[j + 1:close]
            end = close + 1
        else:
            k = j
            while k < n and _is_word(path[k]):
                k += 1
            if k == j:
                i = start + 1
                continue
            name = path[j:k]
            end = k
        if name in os.environ:
            tail = path[end:]
            path = path[:start] + os.environ[name]
            i = len(path)
            path += tail
        else:
            i = end
    return path


def normpath(path):
    path = os.fspath(path)
    if not path:
        return curdir
    initial_slashes = 0
    if path.startswith(sep):
        initial_slashes = 1
        if path.startswith(sep * 2) and not path.startswith(sep * 3):
            initial_slashes = 2
    comps = path.split(sep)
    new_comps = []
    for comp in comps:
        if comp == '' or comp == '.':
            continue
        if (comp != '..' or (initial_slashes == 0 and not new_comps) or
                (new_comps and new_comps[-1] == '..')):
            new_comps.append(comp)
        elif new_comps:
            new_comps.pop()
    path = sep.join(new_comps)
    if initial_slashes:
        path = sep * initial_slashes + path
    return path or curdir


def abspath(path):
    path = os.fspath(path)
    if not isabs(path):
        path = join(os.getcwd(), path)
    return normpath(path)


# CPython's realpath(strict=False): resolve each component in turn,
# following symlinks; a component that does not exist is kept as it is.
def realpath(filename, strict=False):
    filename = os.fspath(filename)
    resolved = {}
    resolving = set()
    path, ok = _joinrealpath("", filename, strict, resolved, resolving)
    return abspath(path)


def _joinrealpath(path, rest, strict, resolved, resolving):
    if isabs(rest):
        rest = rest[1:]
        path = sep
    while rest:
        i = rest.find(sep)
        if i < 0:
            name = rest
            rest = ""
        else:
            name = rest[:i]
            rest = rest[i + 1:]
        if not name or name == curdir:
            continue
        if name == pardir:
            if path:
                path, name = split(path)
                if name == pardir:
                    path = join(path, pardir, pardir)
            else:
                path = pardir
            continue
        newpath = join(path, name)
        m = _mode(newpath, 0)
        if m < 0:
            if strict:
                os.lstat(newpath)
            is_link = False
        else:
            is_link = (m & _S_IFMT) == _S_IFLNK
        if not is_link:
            path = newpath
            continue
        if newpath in resolved:
            path = resolved[newpath]
            continue
        if newpath in resolving:
            # a symlink loop
            if strict:
                os.stat(newpath)
            return (join(newpath, rest), False)
        resolving.add(newpath)
        path, ok = _joinrealpath(path, os.readlink(newpath), strict, resolved, resolving)
        if not ok:
            return (join(path, rest), False)
        resolving.discard(newpath)
        resolved[newpath] = path
    return (path, True)


def relpath(path, start=None):
    path = os.fspath(path)
    if not path:
        raise ValueError("no path specified")
    if start is None:
        s = curdir
    else:
        s = os.fspath(start)
    start_list = [x for x in abspath(s).split(sep) if x]
    path_list = [x for x in abspath(path).split(sep) if x]
    i = 0
    while i < len(start_list) and i < len(path_list) and start_list[i] == path_list[i]:
        i += 1
    rel_list = [pardir] * (len(start_list) - i) + path_list[i:]
    if not rel_list:
        return curdir
    return sep.join(rel_list)


def commonprefix(m):
    if not m:
        return ''
    s1 = min(m)
    s2 = max(m)
    i = 0
    while i < len(s1):
        if s1[i] != s2[i]:
            return s1[:i]
        i += 1
    return s1


def commonpath(paths):
    ps = [os.fspath(p) for p in paths]
    if not ps:
        raise ValueError('commonpath() arg is an empty sequence')
    absolute = ps[0][:1] == sep
    for p in ps:
        if (p[:1] == sep) != absolute:
            raise ValueError("Can't mix absolute and relative paths")
    split_paths = [[c for c in p.split(sep) if c and c != curdir] for p in ps]
    s1 = min(split_paths)
    s2 = max(split_paths)
    common = s1
    i = 0
    while i < len(s1):
        if s1[i] != s2[i]:
            common = s1[:i]
            break
        i += 1
    prefix = sep if absolute else ''
    return prefix + sep.join(common)
