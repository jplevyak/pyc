# Generator expressions aren't implemented yet (issues/008, issues/014).
# Before the issue 008 fix, this crashed the compiler with an internal
# if1_move assertion instead of failing cleanly; this test locks in the
# "clean fail(), not a crash" interim behavior as a regression guard.
g = (x * 2 for x in [1, 2, 3])
for v in g:
    print(v)
