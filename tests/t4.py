def hi(a, b, c):
  if a < b < c: 
    print '< true'
  else:
    print '< false'
  if a > b > c: 
    print '> true'
  else:
    print '> false'
hi(1, 2, 3)
hi(3, 2, 1)
hi(1, 2, 1)
hi(2, 1, 2)
