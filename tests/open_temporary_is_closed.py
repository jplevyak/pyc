# A file object that is only a temporary -- `open(p, 'w').write(x)` -- is
# closed by CPython as soon as the call returns, so a later open of the same
# path sees everything written. pyc kept it open until exit, and an in-process
# read saw only what had been flushed (minpng's PNG, 2727 bytes short). A
# file still bound to a name is not closed, in CPython or here.
open("otc_data_a.bin", "wb").write(b"x" * 10000)
print(len(open("otc_data_a.bin", "rb").read()))
open("otc_data_b.txt", "w").write("")  # start empty: the harness reruns in one directory
for i in range(5):
    open("otc_data_b.txt", "a").write(str(i))
print(open("otc_data_b.txt").read())
f = open("otc_data_c.txt", "w")
f.write("still open")
print(repr(open("otc_data_c.txt").read()))
f.close()
print(repr(open("otc_data_c.txt").read()))
