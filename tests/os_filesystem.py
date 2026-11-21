# os's filesystem calls: results, and the OSError subclass and message on
# failure, as CPython's (issues/202).
import os
import os.path

d = "_os_fs_t"
if os.path.exists(d):
    for top, dirs, files in os.walk(d, topdown=False):
        for f in files:
            os.remove(os.path.join(top, f))
        os.rmdir(top)
os.makedirs(d + "/x/y", exist_ok=True)
os.makedirs(d + "/x/y", exist_ok=True)
try:
    os.makedirs(d + "/x/y")
except FileExistsError as e:
    print("exists:", e)
try:
    os.mkdir(d + "/no/such")
except FileNotFoundError as e:
    print("FNF:", e)
except OSError as e:
    print("OSError??", e)
fh = open(d + "/x/f.txt", "w")
fh.write("hello")
fh.close()
os.mkdir(d + "/z")
print(os.path.exists(d), os.path.isdir(d + "/x"), os.path.isfile(d + "/x/f.txt"), os.path.isfile(d), os.path.getsize(d + "/x/f.txt"))
st = os.stat(d + "/x/f.txt")
print(st.st_size, st[6], len(st), st.st_mtime > 0, isinstance(st.st_mtime, float), st.st_mtime_ns // 1000000000 == st[8])
os.symlink("x", d + "/lnk")
print(os.path.islink(d + "/lnk"), os.path.islink(d + "/x"), os.readlink(d + "/lnk"), os.path.lexists(d + "/lnk"))
print(os.path.realpath(d + "/lnk/f.txt") == os.path.join(os.getcwd(), d, "x/f.txt"))
for top, dirs, files in os.walk(d):
    dirs.sort()
    print(top, dirs, sorted(files))
print(sorted(os.listdir(d)))
os.rename(d + "/x/f.txt", d + "/x/g.txt")
try:
    os.rename(d + "/x/f.txt", d + "/x/h.txt")
except OSError as e:
    print(e)
try:
    os.remove(d + "/nope")
except FileNotFoundError as e:
    print(e)
try:
    os.rmdir(d + "/x")
except OSError as e:
    # ENOTEMPTY's number differs between Linux and macOS
    print(str(e).endswith("Directory not empty: '_os_fs_t/x'"))
try:
    os.listdir(d + "/nope")
except FileNotFoundError as e:
    print(e)
try:
    os.chdir(d + "/x/g.txt")
except NotADirectoryError as e:
    print(e)
cwd = os.getcwd()
os.chdir(d)
print(os.getcwd() == os.path.join(cwd, d), os.path.abspath("q/../r") == os.path.join(cwd, d, "r"))
os.chdir(cwd)
os.remove(d + "/lnk")
os.remove(d + "/x/g.txt")
os.removedirs(d + "/x/y")
os.rmdir(d + "/z")
print(os.path.exists(d))
