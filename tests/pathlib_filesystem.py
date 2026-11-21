# pathlib's concrete paths against the filesystem, as CPython 3.12's PosixPath (issues/202).
from pathlib import Path
import os

base = Path("_pl_fs_t")
if base.exists():
    for top, dirs, files in base.walk(top_down=False):
        for f in files:
            (top / f).unlink()
        for d in dirs:
            if (top / d).is_symlink():
                (top / d).unlink()
            else:
                (top / d).rmdir()
    base.rmdir()
base.mkdir()
(base / "a" / "b").mkdir(parents=True)
(base / "a" / "b").mkdir(parents=True, exist_ok=True)
try:
    (base / "a").mkdir()
except FileExistsError as e:
    print("FileExistsError", e)
try:
    (base / "q" / "r").mkdir()
except FileNotFoundError as e:
    print("FileNotFoundError", e)
print((base / "a" / "x.py").write_text("print(1)\n"), (base / "a" / "b" / "y.py").write_text("yy"))
(base / "a" / "b" / "z.txt").write_bytes(b"\x00\x01zz")
(base / ".hidden.py").touch()
(base / ".hidden.py").touch()
try:
    (base / ".hidden.py").touch(exist_ok=False)
except FileExistsError as e:
    print("FileExistsError", e)
print(repr((base / "a" / "x.py").read_text()), (base / "a" / "b" / "z.txt").read_bytes())
print(base.exists(), base.is_dir(), (base / "a" / "x.py").is_file(), (base / "nope").exists(), (base / "a" / "x.py" / "z").exists())
print(sorted(base.iterdir()), sorted(p.name for p in (base / "a").iterdir()))
(base / "lnk").symlink_to("a")
print((base / "lnk").is_symlink(), (base / "lnk").is_dir(), (base / "a").is_symlink(), (base / "lnk").readlink())
print((base / "lnk" / "x.py").resolve() == (base / "a" / "x.py").resolve(), (base / "lnk" / "x.py").resolve().is_absolute())
print(Path.cwd() == Path(os.getcwd()), base.absolute() == Path.cwd() / base, Path("~").expanduser() == Path.home(),
      Path("~/q").expanduser() == Path.home() / "q", Path("/x/~").expanduser())
st = (base / "a" / "x.py").stat()
print(st.st_size, (base / "a" / "x.py").samefile(base / "lnk" / "x.py"))
newp = (base / "a" / "x.py").rename(base / "a" / "w.py")
print(newp, newp.exists(), (base / "a" / "x.py").exists())
(base / "nope.txt").unlink(missing_ok=True)
try:
    (base / "nope.txt").unlink()
except FileNotFoundError as e:
    print("FileNotFoundError", e)
try:
    (base / "a").rmdir()
except OSError as e:
    # ENOTEMPTY's number differs between Linux and macOS
    print("OSError", str(e).endswith("Directory not empty: '_pl_fs_t/a'"))
with open(base / "a" / "w.py") as fh:
    print(repr(fh.read()), fh.name)
with (base / "a" / "w.py").open() as fh2:
    print(repr(fh2.readline()))
try:
    (base / "missing").read_text()
except FileNotFoundError as e:
    print("FileNotFoundError", e)
print(os.path.exists(base / "a"), os.listdir(base / "a" / "b") != [], os.path.getsize(base / "a" / "w.py"))
for top, dirs, files in base.walk(top_down=False):
    for f in files:
        (top / f).unlink()
    for d in dirs:
        if (top / d).is_symlink():
            (top / d).unlink()
        else:
            (top / d).rmdir()
base.rmdir()
print(base.exists())
