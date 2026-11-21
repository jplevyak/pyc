# Path.glob, rglob and walk, as CPython 3.12's selectors (issues/202).
from pathlib import Path

base = Path("_pl_glob_t")
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
print(sorted(base.glob("*")))
print(sorted(base.glob("*.py")), sorted(base.glob("a/*.py")), sorted(base.glob("*/")), sorted(base.glob("a/*/")))
print(sorted(base.glob("**/*.py")))
print(sorted(base.rglob("*.py")), sorted(base.rglob("*.txt")))
print(sorted(base.glob("**")))
print(sorted(base.glob("a/**/*")), sorted(base.glob("**/b/**/*.txt")))
print(sorted(base.glob("a/../a/x.py")), list((base / "nope").glob("*")))
for top, dirs, files in base.walk():
    dirs.sort()
    print(top, dirs, sorted(files))
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
