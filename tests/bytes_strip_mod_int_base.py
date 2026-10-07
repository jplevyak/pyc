# msp_ss's Intel-HEX reader and frame builder: bytes.strip/lstrip,
# `b'%c' % value` with a single non-tuple operand, and int(bytes, base).
line = b":10010000214601360121470136007EFE09D2190140\r\n"
s = line.strip()
print(s, b"  x  ".lstrip(), b"  x  ".rstrip(), b"\t y\n".strip())
print(int(s[1:3], 16), int(s[3:7], 16), int(b"ff", 16), int("-12", 10))
print(b"%c" % 65, b"%c%c" % (66, 67), b"%d" % 7, b"%c" % (0x41 & 0xff))
c = 0x7e
print(b"%c" % c)
