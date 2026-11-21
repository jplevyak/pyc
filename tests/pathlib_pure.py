# pathlib's pure paths, as CPython 3.12's PurePosixPath (issues/202).
from pathlib import Path, PurePath, PurePosixPath, PosixPath
import os

for s in ["a/b/c.txt", "/usr/lib/x.tar.gz", "//srv/x", "///a", "", ".", "a/./b//c/", "..", "../a", ".bashrc", "x.", "/"]:
    p = PurePath(s)
    print(repr(p), str(p), p.name, repr(p.suffix), p.suffixes, repr(p.stem), p.parts, repr(p.parent), p.is_absolute(),
          repr(p.root), repr(p.anchor), list(p.parents), len(p.parents))
p = PurePath("a", "b", "/c", "d")
print(p, PurePath("x") / "y" / PurePath("z"), "q" / PurePath("r"), PurePath("a").joinpath("b", "c"))
print(PurePath("a/b.py").with_suffix(".txt"), PurePath("a/b").with_suffix(".x"), PurePath("a/b.tar.gz").with_suffix(""),
      PurePath("a/b.py").with_name("c.md"), PurePath("a/b.py").with_stem("z"))
for bad in ["", "x/y", "."]:
    try:
        PurePath("a/b").with_name(bad)
    except ValueError as e:
        print("ValueError", e)
try:
    PurePath("/").with_name("x")
except ValueError as e:
    print("ValueError", e)
try:
    PurePath("a/b").with_suffix("txt")
except ValueError as e:
    print("ValueError", e)
print(PurePath("/a/b/c").relative_to("/a"), PurePath("/a/b").relative_to("/a/b"), PurePath("/a/b").is_relative_to("/a"),
      PurePath("/a/b").is_relative_to("/c"))
try:
    PurePath("/a/b").relative_to("/c")
except ValueError as e:
    print("ValueError", e)
print(PurePath("/a/b").relative_to("/a/c", walk_up=True))
print(PurePath("a/b.py").match("*.py"), PurePath("/a/b/c.py").match("b/*.py"), PurePath("/a/b/c.py").match("/*.py"),
      PurePath("a/b.py").match("a/*/b.py"), PurePath("/a/b.py").match("/a/*.py"), PurePath("ab").match("[!a]b"))
print(PurePath("a") == PurePath("a"), PurePath("a") == PurePath("a/"), PurePath("a") == "a", PurePath("a") != PurePath("b"),
      PurePath("a") < PurePath("b"), PurePath("a/b") > PurePath("a"), hash(PurePath("a/b")) == hash(PurePath("a//b")),
      sorted([PurePath("b"), PurePath("a/c"), PurePath("a")]))
print(isinstance(Path("x"), PurePath), isinstance(PurePath("x"), Path), isinstance(PosixPath("x"), PurePosixPath),
      repr(PurePosixPath("x")), repr(PosixPath("y")), PurePath("a") == Path("a"), repr(Path()), repr(PurePath("a").parents))
ss = {PurePath("a"), PurePath("a/"), PurePath("b")}
print(len(ss), str(Path("q")), f"{Path('f')}", os.fspath(Path("g")), os.path.join(Path("h"), "i"))

class MyPath(Path):
    def shout(self):
        return str(self).upper()
m = MyPath("x/y")
print(repr(m), repr(m.parent), repr(m / "z"), (m / "z").shout(), isinstance(m, Path))
