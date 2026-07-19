# issues/025: `a[::-1]` on a plain list hung (not just wrong output).
# Root cause chain, three separate bugs found while fixing it:
#
# 1. _CG_list_getslice_internal/_CG_string_getslice had no real
#    negative-step support -- an omitted bound always defaulted to
#    the positive-step values (0/len), so a reverse slice computed a
#    negative element count that, stored into the list's (unsigned)
#    length header, wrapped to a huge value -- reading/printing it
#    was practically infinite, not just incorrect.
# 2. _CG_list_getslice_internal separately had a signed/unsigned
#    comparison bug: `len` was uint32, so `l > len` on a negative
#    int32 `l` promoted `l` to a huge unsigned value first, comparing
#    true and clamping `l` to `len` *before* the negative-index branch
#    ever ran -- `a[-3:]` on a 5-element list returned `[]`.
# 3. int64_constant()'s Immediate is a union of v_int32/v_int64; the
#    constructor only ever set v_int32, so a negative value read back
#    as int64 (the frontend's new INT_MIN "omitted lower bound"
#    sentinel, needed to fix bug 1) saw v_int64's untouched upper
#    bits -- INT_MIN emitted into generated C as its unsigned
#    magnitude, 2147483648, not -2147483648.
a = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
print(a[::-1])
print(a[::2])
print(a[::-2])
print(a[8:2:-1])
print(a[2:8:-1])
print(a[-1::-1])
print(a[:-3:-1])
print(a[-3:])
print(a[-3:-1])
print(a[100:0:-1])
print(a[0:100:-1])

s = "hello world"
print(s[::-1])
print(s[::-2])
print(s[8:2:-1])
print(s[-1::-1])
print(s[-3:])
print(s[-3:-1])

b = [1, 2, 3, 4, 5]
c = b[:]
c[0] = 99
print(b)
print(c)
