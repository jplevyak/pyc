# bytes %-formatting with a tuple of mixed argument types (minilight's PPM
# header): the tuple is converted position by position
# (tuple.__pyc_bytes_fmtargs__), since a mixed record cannot be read at a
# runtime index (ifa/134). And %s / %b, which the formatter lacked.
PPM_ID = b'P6'
URI = b'http://www.hxa7241.org/minilight/'
w, h = 64, 48
print(b'%s\n# %s\n\n%u %u\n255\n' % (PPM_ID, URI, w, h))
print(b'%c%c%c%c' % (65, 66, 67, 68), b'%c' % 69, b'%c' % b'Z')
print(b'%d %i %u' % (7, True, 3.9), b'%%d', b'%s' % b'one', b'%b-%d' % (b'x', 2))
