# A failed open() raises the OSError subclass CPython raises, with its
# message. It used to return a 0 handle unchecked, and the first read
# segfaulted (shedskin_examples/doom without DOOM1.WAD).
for path, mode in [("no_such_file.txt", "r"), ("/", "r"), ("no_dir/x.txt", "w")]:
    try:
        open(path, mode)
        print("opened", path)
    except FileNotFoundError as e:
        print("FileNotFoundError", e)
    except IsADirectoryError as e:
        print("IsADirectoryError", e)

# The binary path (open(..., 'rb') lowers to open_binary) and the OSError
# base class.
try:
    open("no_such.bin", "rb")
except OSError as e:
    print("OSError", e)

# A literal 'rb' mode is lowered to open_binary by the frontend, which
# returned before the exception check: even an exact handler missed it.
try:
    open("no_such2.bin", "rb")
except FileNotFoundError as e:
    print("FileNotFoundError", e)

# An open that succeeds is unchanged.
f = open("open_failure_raises.py")
print(f.readline()[:10])
f.close()
