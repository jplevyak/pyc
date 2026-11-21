# 202 — `os`, `os.path` and `pathlib`

**Status:** open for its residuals. Landed 2026-11-20, the second of the
widely used modules chosen in [196](196-compiling-shedskin-with-pyc.md)
(shedskin imports `pathlib` in ten modules), after
[197](197-logging.md).

## What is there

POSIX only. For what each covers, the output is CPython 3.12's byte for
byte, on both backends and under `--strict`.

- **`os`** is a package now: `pyc_lib/os/__init__.py` and
  `pyc_lib/os/path.py`, so `import os.path` and `from os.path import join`
  work. The calls: `getcwd`, `chdir`, `listdir`, `mkdir`, `makedirs`
  (`exist_ok`), `rmdir`, `removedirs`, `remove`/`unlink`, `rename`/
  `replace`, `symlink`, `readlink`, `stat`/`lstat` (an `os.stat_result`
  with the `st_*` fields, float `st_*time`, `st_*time_ns`, and CPython's
  ten-field tuple view), `walk` (a generator, top-down with pruning or
  bottom-up, `followlinks`, `onerror`), `access`, `system`, `getpid`,
  `cpu_count`, `fspath`, `strerror`, `environ` (the real environment;
  writes and deletes reach `setenv`/`unsetenv`), `getenv`, `putenv`,
  `unsetenv`, and the constants (`sep`, `curdir`, `devnull`, ...).
- **A failed call raises CPython's `OSError` subclass** with CPython's
  message: `[Errno 2] No such file or directory: 'x'`, `... 'a' -> 'b'`
  for two paths. `FileExistsError`, `NotADirectoryError` and the other
  `OSError` subclasses were added to the builtin exceptions.
- **`os.path`** is a port of posixpath: `join` (any number of parts),
  `split`, `splitext`, `splitdrive`, `splitroot`, `basename`, `dirname`,
  `isabs`, `normpath`, `abspath`, `realpath` (CPython's component walk),
  `relpath`, `commonprefix`, `commonpath`, `exists`, `lexists`, `isdir`,
  `isfile`, `islink`, `getsize`, `getmtime`/`getatime`/`getctime`,
  `samefile`, `expanduser` (`~` and `~user`), `expandvars`, `normcase`.
- **`pathlib`**: `PurePath` and `Path` (also under their POSIX names) with
  `/` from either side, `parts`, `name`, `suffix`, `suffixes`, `stem`,
  `parent`, `parents`, `with_name`/`with_stem`/`with_suffix`,
  `relative_to` (with `walk_up`), `is_relative_to`, `match`, `joinpath`,
  ordering, hashing and equality; and `cwd`, `home`, `exists`, `is_dir`,
  `is_file`, `is_symlink`, `stat`, `lstat`, `samefile`, `open`,
  `read_text`/`write_text`, `read_bytes`/`write_bytes`, `iterdir`, `walk`,
  `glob` and `rglob` (CPython 3.12's selectors, `**` included),
  `absolute`, `resolve`, `readlink`, `touch`, `mkdir` (`parents`,
  `exist_ok`), `unlink` (`missing_ok`), `rmdir`, `rename`, `replace`,
  `symlink_to`, `expanduser`. A subclass of `Path` derives paths of its
  own class, as 3.12's does.
- **`open()` takes a path object**, as CPython's does (`__pyc_fspath__`).
- **`fnmatch`** was rewritten to CPython's `translate` exactly: it negated
  on `[^...]` (CPython's `^` is a member), ended a set at a leading `]`,
  and `fnmatch()` lowercased both sides, which POSIX does not.

## Found and fixed on the way

- **Circular imports crashed the compiler** (a stack overflow):
  `build_one_module_if1` marked a module built after building it, so `pk`
  importing `pk.sub` importing `pk` recursed. It marks it first now.
  CPython's own `os` and `posixpath` import each other.
- **Reflected operators** did not exist for user classes: `"dir" / path`
  was an unresolved `__truediv__` on str. `object` (and str, for the
  operators it lacks) now has CPython's fallback for each binary operator (`a + b` with no `__add__`
  on `a` calls `b.__radd__(a)`). Not modelled: a forward method returning
  `NotImplemented`, and the rule that asks a right operand of a SUBCLASS
  first. `list.__radd__`, a stub returning None, was removed.
- **A file's `write()` returned None**; it returns the count, as CPython's.
- **`os` was a stub in places** (issues/041): `os.path` was an object,
  `join` took two arguments, a failed `mkdir`, `chdir`, `remove` or
  `rename` returned an int instead of raising, `stat` returned a plain
  tuple, `walk` returned a list in the wrong order, and `environ` was a
  dict holding one invented entry.
- A frontend addition serves library code: `__pyc_class_name__` and
  `__pyc_class__` (`type(self).__name__` and `type(self)`) are
  synthesized in every subclass of a class that defines them
  (PYTHON_FRONTEND.md §13.10). The first version synthesized them in
  EVERY class and cost the corpus +17129 CreationSets (+13.6 %, about 180
  a program, all prototype closures for methods nothing called; sweep
  `check__default__5ad2dc46+03b2a253`), so it is opt-in by inheritance.

Filed, not fixed: [203](203-str-counts-utf8-bytes.md) (a str's `len` and
indexing count UTF-8 bytes) and [204](204-partly-initialized-module-read.md)
(a circular import's early read). [175](175-an-exception-raised-in-an-operator-method-does-not-propagate.md)
gained a finding: `dict.__getitem__` on a missing key returns a value and
raises nothing.

## Refused

| what | how | why |
| --- | --- | --- |
| bytes paths | compile error (no `__fspath__` on bytes) | not ported; every call converts with `str` C calls |
| `e.errno`, `e.strerror`, `e.filename` on an `OSError` | compile error (unresolved member) | pyc's exceptions carry one message string |
| `PureWindowsPath`, `WindowsPath`, `as_uri`, `owner`, `group`, `chmod`, `lchmod`, `is_mount`, `is_junction`, `is_block_device` and the other file-type tests, `hardlink_to`, `case_sensitive=` | compile error (unknown name, member or keyword) | not ported |
| `class C(PosixPath)` or `class C(PurePosixPath)` | compile error (the base is not a class) | the two are aliases of `Path` and `PurePath` here; subclass those |
| `Path.open()` in a binary mode | `NotImplementedError` when called | pyc chooses a binary file from a literal mode at the `open()` call, and Path.open's mode is a variable; `read_bytes()`, `write_bytes()` and `open(path, 'rb')` work |
| an encoding other than UTF-8, `errors=` or `newline=` in `open`/`read_text`/`write_text` | `NotImplementedError` when called | a pyc str is UTF-8 bytes (203) |
| `os.scandir`, `os.utime`, `os.chmod`, `os.environb`, `os.fsencode`, ... | compile error | not ported |
| a `KeyError` from `os.environ[k]` caught by `except` | the handler does not run | an item subscript's raise does not propagate (175); `get`, `pop` and `in` work |

## Deviations

`PurePath is PurePosixPath` and `Path is PosixPath` are True here (False
in CPython): one class answers to both names. Nothing else that pyc
compiles can tell them apart, because `type()` is refused.

## Corpus

`check__default__5ad2dc46+f9726c92` matches
`check__default__ce537989+8714296f` on every verdict (77 compile, none fail
to run, stdout as before: only othello2 differs, issues/177), and compile
wall time summed over the corpus is 1194 s against 1256 s. Container
CreationSets are unchanged except block -1, plcfrs -1, rdb +4 (rdb is the
one corpus program using `os` heavily: `stat`, `rename`, `chdir`). Every
program gains a fixed 12 EntrySets and about 65 CreationSets from the
builtin module, measured on sieve: the twelve new `OSError` subclasses
+12 ES / +48 CS (a builtin class's prototype is built whether or not the
program uses it, like every other exception class), the reflected
operators on `object` +13 CS, the rest 4. The first version, which
synthesized the class-name methods in every class, measured +180 CS a
program (`check__default__5ad2dc46+03b2a253`) and was made opt-in.

## Tests

`os_path_pure`, `os_filesystem`, `pathlib_pure`, `pathlib_filesystem`,
`pathlib_glob`, `fnmatch_sets`, `reflected_operators`, `import_circular`
and `file_write_count` match CPython on both backends and under
`--strict`; `pathlib_open_binary_refused` checks the refusal.
