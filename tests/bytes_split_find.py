# bytes.split / bytes.find, including embedded NULs (rdb: entry[33::2].split(b"\0", 1)).
e = b"n\x00a\x00m\x00e\x00\x00\x00z\x00"
print(e[0::2].split(b"\0", 1), e[0::2].split(b"\0", 1)[0], b"a,b,,c".split(b","), b" x\ty \n".split(), b" x  y ".split(None, 1))
print(b"hello".find(b"l"), b"hello".find(b"l", 3), b"hello".find(b"z"), b"\x00\x01\x00".find(b"\x00", 1))
