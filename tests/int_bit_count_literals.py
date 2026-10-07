# int.bit_count() (othello2's bitboards), and integer literals: `0b`/`0o`
# prefixes were parsed as base 10 and silently became 0 (othello2's
# `move & 0b111`), and `_` digit separators did not parse at all.
print((0).bit_count(), (7).bit_count(), (-7).bit_count(), (2**62 + 1).bit_count())
print(0b1011, 0B11, 0o17, 0O7, 0x1F, 0XfF, 0)
print(1_000, 0b_1_1, 0x_ff, 0o_17, 0xDEAD_BEEF)
print(1_0.5, 1e1_0, 1_000.25e-1_0, .5, 5.)
move = 0b101011
print(move & 0b111, (move >> 3) & 0b111, f"{move.bit_count()}")
