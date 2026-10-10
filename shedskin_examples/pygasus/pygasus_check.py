# pyc check driver for pygasus (added, not an upstream file: see
# ../PYC_CHANGES.md). pygasus's own __main__ is `while True: print(x);
# pExec()`, so all it prints is a counter, and it never ends. This runs the
# same setup for a fixed number of frames and prints the emulator's state
# at checkpoints, so its output can be compared with CPython's.
# corpus_sweep.sh runs this in place of pygasus.py in run/check mode.
import pygasus

FRAMES = 1500
EVERY = 100


def h(values):
    r = 0
    for v in values:
        r = (r * 31 + v) & 0xffffffff
    return r


pygasus.read_ines('mario_bros.nes')
pygasus.setkeys({'q': 0, 'w': 0, 'a': 0, 's': 0, 'UP': 0, 'DOWN': 0, 'LEFT': 0, 'RIGHT': 0})
pygasus.setkeys2((0,))
pygasus.getscreen()
pygasus.pReset()
for frame in range(1, FRAMES + 1):
    pygasus.pExec()
    if frame % EVERY == 0:
        print("frame", frame,
              "A", pygasus.A, "X", pygasus.X, "Y", pygasus.Y,
              "PC", pygasus.PC, "P", pygasus.P, "S", pygasus.S,
              "ram", h(pygasus.nesRAM), "ppu", h(pygasus.ppuRAM),
              "spr", h(pygasus.SPRRAM), "screen", h(pygasus.getscreen()))
