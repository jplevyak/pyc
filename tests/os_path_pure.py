# os.path as CPython's posixpath, and os.environ (issues/202).
import os
import os.path
from os.path import join, splitext

print(join("a", "b", "c"), join("a/", "/b", "c"), join("", "x"), os.path.join("a"))
for p in ["a/b.txt", "/a/b", "/", "//a", "a/", "a//", ".bashrc", "a/.b.c", "a.b/c", "..", "x.", "/a/b/.."]:
    print(repr(p), os.path.split(p), splitext(p), os.path.basename(p), repr(os.path.dirname(p)),
          os.path.normpath(p), os.path.isabs(p), os.path.splitroot(p))
print(os.path.normpath("///a//b/./c/../d"), os.path.normpath("../../a/.."), os.path.normpath(""))
print(os.path.commonprefix(["/usr/lib", "/usr/local"]), os.path.commonpath(["/usr/lib", "/usr/local"]))
print(os.path.relpath("/a/b/c", "/a/d"), os.path.relpath("a/b", "a/b"), os.path.relpath("x", "y/z"))
os.environ["PYC_T_V"] = "val"
print(os.environ["PYC_T_V"], os.getenv("PYC_T_V"), os.getenv("PYC_T_NOPE"), os.getenv("PYC_T_NOPE", "dflt"),
      "PYC_T_V" in os.environ, os.path.expandvars("$PYC_T_V/${PYC_T_V}/$NOPE_X/${}/$"))
print(os.path.expanduser("~") == os.environ["HOME"], os.path.expanduser("~nosuchuserxyz/a"))
del os.environ["PYC_T_V"]
print("PYC_T_V" in os.environ)
try:
    print(os.environ.pop("PYC_T_V"))
except KeyError as e:
    print("KeyError", e)

