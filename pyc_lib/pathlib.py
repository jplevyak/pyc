# pyc shim for the standard `pathlib` module, POSIX only (issues/202).
#
# A port of CPython 3.12's pathlib for its POSIX flavour: PurePath and Path,
# with the same parsing, the same results and the same exceptions.
#
# How it differs in shape, not in answers:
#
# - CPython's PurePath(...) and Path(...) return a PurePosixPath and a
#   PosixPath through __new__. Here PurePosixPath and PosixPath are the same
#   classes as PurePath and Path, which answer to those names
#   (__pyc_class_name__), so repr, isinstance and the results of every
#   method are CPython's. `PurePath is PurePosixPath` is True here, and a
#   class can subclass PurePath or Path but not the two aliases (a compile
#   error: an alias is not a class to pyc).
# - A subclass's derived paths (`/`, parent, iterdir, ...) are of the
#   subclass, as CPython 3.12's are (`__pyc_class__` is type(self), and the frontend synthesizes it in
#   each subclass).
#
# Not here, each a compile error (an unknown name or member) unless noted:
# PureWindowsPath and WindowsPath, as_uri, owner and group, chmod and
# lchmod, is_mount, is_junction, the block/char/fifo/socket tests,
# hardlink_to, the `case_sensitive` keyword, and Path.open in a binary mode
# or with an encoding other than UTF-8 (NotImplementedError when called).

import os
import os.path
from fnmatch import fnmatchcase

_S_IFMT = 0o170000
_S_IFDIR = 0o040000
_S_IFREG = 0o100000
_S_IFLNK = 0o120000


def _parse_path(path):
    if not path:
        return ("", [])
    drv, root, rel = os.path.splitroot(path)
    tail = [x for x in rel.split('/') if x and x != '.']
    return (root, tail)


def _format_parsed_parts(root, tail):
    return root + '/'.join(tail)


class PurePath:
    def __init__(self, *args):
        raw = []
        for a in args:
            raw.append(os.fspath(a))
        if len(raw) == 0:
            path = ''
        elif len(raw) == 1:
            path = raw[0]
        else:
            path = raw[0]
            k = 1
            while k < len(raw):
                path = os.path.join(path, raw[k])
                k += 1
        root, tail = _parse_path(path)
        self._root = root
        self._tail = tail
        self._str = _format_parsed_parts(root, tail) or '.'

    # type(self).__name__ and type(self). Defining them here is what makes
    # the frontend synthesize them in every subclass, which answers with its
    # own name and class (PYTHON_FRONTEND.md section 13.10).
    def __pyc_class_name__(self):
        return "PurePosixPath"

    def __pyc_class__(self):
        return PurePath

    def with_segments(self, *pathsegments):
        return self.__pyc_class__()(*pathsegments)

    def _from_parsed_parts(self, root, tail):
        return self.with_segments(_format_parsed_parts(root, tail))

    def __str__(self):
        return self._str

    def __fspath__(self):
        return self._str

    def as_posix(self):
        return self._str

    def __repr__(self):
        return self.__pyc_class_name__() + "(" + repr(self._str) + ")"

    def __hash__(self):
        return hash(self._str)

    def __eq__(self, other):
        if not isinstance(other, PurePath):
            return False
        return self._str == other._str

    def __ne__(self, other):
        return not self.__eq__(other)

    def _parts_normcase(self):
        return self._str.split('/')

    def __lt__(self, other):
        return self._parts_normcase() < other._parts_normcase()

    def __le__(self, other):
        return self._parts_normcase() <= other._parts_normcase()

    def __gt__(self, other):
        return self._parts_normcase() > other._parts_normcase()

    def __ge__(self, other):
        return self._parts_normcase() >= other._parts_normcase()

    @property
    def drive(self):
        return ""

    @property
    def root(self):
        return self._root

    @property
    def anchor(self):
        return self._root

    @property
    def name(self):
        tail = self._tail
        if not tail:
            return ''
        return tail[-1]

    @property
    def suffix(self):
        name = self.name
        i = name.rfind('.')
        if 0 < i and i < len(name) - 1:
            return name[i:]
        return ''

    @property
    def suffixes(self):
        name = self.name
        if name.endswith('.'):
            return []
        name = name.lstrip('.')
        return ['.' + suffix for suffix in name.split('.')[1:]]

    @property
    def stem(self):
        name = self.name
        i = name.rfind('.')
        if 0 < i and i < len(name) - 1:
            return name[:i]
        return name

    @property
    def parts(self):
        if self._root:
            return tuple([self._root] + self._tail)
        return tuple(self._tail)

    @property
    def parent(self):
        tail = self._tail
        if not tail:
            return self
        return self._from_parsed_parts(self._root, tail[:-1])

    @property
    def parents(self):
        return _PathParents(self)

    def with_name(self, name):
        if not self.name:
            raise ValueError(repr(self) + " has an empty name")
        if not name or '/' in name or name == '.':
            raise ValueError("Invalid name " + repr(name))
        return self._from_parsed_parts(self._root, self._tail[:-1] + [name])

    def with_stem(self, stem):
        return self.with_name(stem + self.suffix)

    def with_suffix(self, suffix):
        if '/' in suffix:
            raise ValueError("Invalid suffix " + repr(suffix))
        if (suffix and not suffix.startswith('.')) or suffix == '.':
            raise ValueError("Invalid suffix " + repr(suffix))
        name = self.name
        if not name:
            raise ValueError(repr(self) + " has an empty name")
        old_suffix = self.suffix
        if not old_suffix:
            name = name + suffix
        else:
            name = name[:-len(old_suffix)] + suffix
        return self._from_parsed_parts(self._root, self._tail[:-1] + [name])

    def joinpath(self, *pathsegments):
        return self.with_segments(self, *pathsegments)

    def __truediv__(self, key):
        return self.with_segments(self, key)

    def __rtruediv__(self, key):
        return self.with_segments(key, self)

    def is_absolute(self):
        return self._root != ""

    def is_reserved(self):
        return False

    def is_relative_to(self, other):
        other = self.with_segments(other)
        if other == self:
            return True
        for p in self.parents:
            if p == other:
                return True
        return False

    def relative_to(self, other, walk_up=False):
        other = self.with_segments(other)
        candidates = [other]
        for p in other.parents:
            candidates.append(p)
        step = 0
        found = -1
        while step < len(candidates):
            path = candidates[step]
            if self.is_relative_to(path):
                found = step
                break
            elif not walk_up:
                raise ValueError(repr(str(self)) + " is not in the subpath of " + repr(str(other)))
            elif path.name == '..':
                raise ValueError("'..' segment in " + repr(str(other)) + " cannot be walked")
            step += 1
        if found < 0:
            raise ValueError(repr(str(self)) + " and " + repr(str(other)) + " have different anchors")
        path = candidates[found]
        parts = ['..'] * found + self._tail[len(path._tail):]
        return self.with_segments(*parts)

    # CPython 3.12 compiles the pattern to one regex over the path's
    # components; for a POSIX path that is each pattern component matched by
    # fnmatch against the path's, aligned from the right, and all of them
    # for an absolute pattern.
    def match(self, path_pattern):
        pat = self.with_segments(path_pattern)
        pparts = pat._tail
        if not pat._root and not pparts:
            raise ValueError("empty pattern")
        parts = self._tail
        if pat._root:
            if self._root != pat._root or len(parts) != len(pparts):
                return False
        elif len(pparts) > len(parts):
            return False
        k = 1
        while k <= len(pparts):
            if not _part_match(parts[-k], pparts[-k]):
                return False
            k += 1
        return True


# CPython 3.12 reads a lone `*` component of a match pattern as `.+` and
# every other component through fnmatch.translate.
def _part_match(name, pat):
    if pat == '*':
        return len(name) > 0
    return fnmatchcase(name, pat)


class _PathParents:
    def __init__(self, path):
        self._path = path
        self._root = path._root
        self._tail = path._tail

    def __len__(self):
        return len(self._tail)

    def __getitem__(self, idx):
        n = len(self._tail)
        if idx >= n or idx < -n:
            raise IndexError(idx)
        if idx < 0:
            idx += n
        return self._path._from_parsed_parts(self._root, self._tail[:-idx - 1])

    def __iter__(self):
        n = len(self._tail)
        out = []
        idx = 0
        while idx < n:
            out.append(self._path._from_parsed_parts(self._root, self._tail[:-idx - 1]))
            idx += 1
        return iter(out)

    def __repr__(self):
        return "<" + self._path.__pyc_class_name__() + ".parents>"


def _is_utf8(encoding):
    if encoding is None:
        return True
    e = encoding.lower().replace('_', '-')
    return e == 'utf-8' or e == 'utf8'


class Path(PurePath):
    def __pyc_class_name__(self):
        return "PosixPath"

    def __pyc_class__(self):
        return Path

    @classmethod
    def cwd(cls):
        return cls(os.getcwd())

    @classmethod
    def home(cls):
        return cls("~").expanduser()

    def stat(self, follow_symlinks=True):
        return os.stat(self._str, follow_symlinks)

    def lstat(self):
        return os.stat(self._str, False)

    # The mode of the file, -1 for an error CPython's predicates read as
    # False (ENOENT, ENOTDIR, EBADF, ELOOP); any other error raises.
    def _mode(self, follow):
        e = __pyc_c_call__(int, "_CG_os_stat", str, self._str, int, follow)
        if e != 0:
            if __pyc_c_call__(int, "_CG_os_ignorable_errno", int, e) != 0:
                return -1
            os._fail(e, self._str)
        return __pyc_c_call__(int, "_CG_os_stat_int", int, 0)

    def exists(self, follow_symlinks=True):
        return self._mode(1 if follow_symlinks else 0) >= 0

    def is_dir(self):
        m = self._mode(1)
        return m >= 0 and (m & _S_IFMT) == _S_IFDIR

    def is_file(self):
        m = self._mode(1)
        return m >= 0 and (m & _S_IFMT) == _S_IFREG

    def is_symlink(self):
        m = self._mode(0)
        return m >= 0 and (m & _S_IFMT) == _S_IFLNK

    def samefile(self, other_path):
        st = self.stat()
        other_st = os.stat(other_path)
        return st.st_ino == other_st.st_ino and st.st_dev == other_st.st_dev

    def open(self, mode='r', buffering=-1, encoding=None, errors=None, newline=None):
        if 'b' in mode:
            raise NotImplementedError("pyc: Path.open() in a binary mode; use read_bytes(), write_bytes() or open(path, 'rb') (issues/202)")
        if not _is_utf8(encoding) or errors is not None or newline is not None:
            raise NotImplementedError("pyc: Path.open() with an encoding other than UTF-8, errors or newline (issues/202)")
        return open(self._str, mode)

    def read_bytes(self):
        f = open(self._str, 'rb')
        data = f.read()
        f.close()
        return data

    def read_text(self, encoding=None, errors=None):
        if not _is_utf8(encoding) or errors is not None:
            raise NotImplementedError("pyc: Path.read_text() with an encoding other than UTF-8, or errors (issues/202)")
        f = open(self._str, 'r')
        data = f.read()
        f.close()
        return data

    def write_bytes(self, data):
        f = open(self._str, 'wb')
        n = f.write(data)
        f.close()
        return n

    def write_text(self, data, encoding=None, errors=None, newline=None):
        if not _is_utf8(encoding) or errors is not None or newline is not None:
            raise NotImplementedError("pyc: Path.write_text() with an encoding other than UTF-8, errors or newline (issues/202)")
        f = open(self._str, 'w')
        n = f.write(data)
        f.close()
        return n

    def _make_child_relpath(self, name):
        if self._tail:
            return self.with_segments(self._str + '/' + name)
        if self._str != '.':
            return self.with_segments(self._str + name)
        return self.with_segments(name)

    def iterdir(self):
        for name in os.listdir(self._str):
            yield self._make_child_relpath(name)

    # Path.walk: CPython's explicit stack, whose entries here are
    # (path, dirnames, filenames, is_result) rather than a path or a tuple.
    def walk(self, top_down=True, on_error=None, follow_symlinks=False):
        stack = [(self, [], [], False)]
        while stack:
            path, dn, fn, is_result = stack.pop()
            if is_result:
                yield (path, dn, fn)
                continue
            try:
                names = os.listdir(path._str)
            except OSError as error:
                if on_error is not None:
                    on_error(error)
                continue
            dirnames = []
            filenames = []
            for name in names:
                full = path._str + '/' + name
                m = os.path._mode(full, 1 if follow_symlinks else 0)
                if m >= 0 and (m & _S_IFMT) == _S_IFDIR:
                    dirnames.append(name)
                else:
                    filenames.append(name)
            if top_down:
                yield (path, dirnames, filenames)
            else:
                stack.append((path, dirnames, filenames, True))
            k = len(dirnames) - 1
            while k >= 0:
                stack.append((path._make_child_relpath(dirnames[k]), [], [], False))
                k -= 1

    def glob(self, pattern):
        if not pattern:
            raise ValueError("Unacceptable pattern: " + repr(pattern))
        root, parts = _parse_path(pattern)
        if root:
            raise NotImplementedError("Non-relative patterns are unsupported")
        if pattern[-1] == '/':
            parts.append('')
        if not self.is_dir():
            return
        for p in _select(self, parts, 0):
            yield p

    def rglob(self, pattern):
        root, parts = _parse_path(pattern)
        if root:
            raise NotImplementedError("Non-relative patterns are unsupported")
        if pattern and pattern[-1] == '/':
            parts.append('')
        if not self.is_dir():
            return
        for p in _select(self, ['**'] + parts, 0):
            yield p

    def absolute(self):
        if self.is_absolute():
            return self
        cwd = os.getcwd()
        if not self._tail:
            return self.with_segments(cwd)
        return self.with_segments(cwd, self)

    def resolve(self, strict=False):
        return self.with_segments(os.path.realpath(self._str, strict))

    def readlink(self):
        return self.with_segments(os.readlink(self._str))

    def touch(self, mode=0o666, exist_ok=True):
        if exist_ok:
            if __pyc_c_call__(int, "_CG_os_utime_now", str, self._str) == 0:
                return
        e = __pyc_c_call__(int, "_CG_os_create", str, self._str, int, mode, int, 0 if exist_ok else 1)
        if e != 0:
            os._fail(e, self._str)

    def mkdir(self, mode=0o777, parents=False, exist_ok=False):
        try:
            os.mkdir(self._str, mode)
        except FileNotFoundError:
            if not parents or self.parent == self:
                raise
            self.parent.mkdir(parents=True, exist_ok=True)
            self.mkdir(mode, parents=False, exist_ok=exist_ok)
        except OSError:
            if not exist_ok or not self.is_dir():
                raise

    def unlink(self, missing_ok=False):
        try:
            os.unlink(self._str)
        except FileNotFoundError:
            if not missing_ok:
                raise

    def rmdir(self):
        os.rmdir(self._str)

    def rename(self, target):
        os.rename(self._str, target)
        return self.with_segments(target)

    def replace(self, target):
        os.replace(self._str, target)
        return self.with_segments(target)

    def symlink_to(self, target, target_is_directory=False):
        os.symlink(target, self._str)

    def expanduser(self):
        if not self._root and self._tail and self._tail[0][:1] == '~':
            homedir = os.path.expanduser(self._tail[0])
            if homedir[:1] == "~":
                raise RuntimeError("Could not determine home directory.")
            root, tail = _parse_path(homedir)
            return self._from_parsed_parts(root, tail + self._tail[1:])
        return self


PurePosixPath = PurePath
PosixPath = Path


# glob's selectors (CPython 3.12's _make_selector), one generator walking
# the pattern's components.
def _select(path, parts, i):
    if i == len(parts) or parts[i] == '':
        yield path
        return
    pat = parts[i]
    if pat == '**':
        j = i
        while j < len(parts) and parts[j] == '**':
            j += 1
        dedupe = '**' in parts[j:]
        seen = set()
        for start in _iterate_directories(path):
            for p in _select(start, parts, j):
                if dedupe:
                    if p in seen:
                        continue
                    seen.add(p)
                yield p
    elif pat == '..':
        for p in _select(path._make_child_relpath('..'), parts, i + 1):
            yield p
    else:
        if '**' in pat:
            raise ValueError("Invalid pattern: '**' can only be an entire path component")
        dironly = i + 1 < len(parts)
        try:
            names = os.listdir(path._str)
        except OSError:
            return
        for name in names:
            if dironly:
                m = os.path._mode(path._str + '/' + name, 1)
                if m < 0 or (m & _S_IFMT) != _S_IFDIR:
                    continue
            if fnmatchcase(name, pat):
                for p in _select(path._make_child_relpath(name), parts, i + 1):
                    yield p


def _iterate_directories(path):
    yield path
    for dirpath, dirnames, filenames in path.walk():
        for dirname in dirnames:
            yield dirpath._make_child_relpath(dirname)
