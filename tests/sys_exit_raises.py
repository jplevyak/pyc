# sys.exit() raises SystemExit, as in CPython, so a program can catch it
# (shedskin_examples/kanoodle ends each search this way). It was a hard C
# exit: kanoodle stopped after its first search, silently, with rc 0.
import sys
def f():
    print('in f')
    sys.exit()
for i in range(2):
    try:
        print('begin', i)
        f()
        print('not reached')
    except SystemExit:
        print('caught')
print('end')
